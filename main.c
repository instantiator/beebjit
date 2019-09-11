#include "bbc.h"
#include "cpu_driver.h"
#include "keyboard.h"
#include "os_poller.h"
#include "os_sound.h"
#include "os_window.h"
#include "sound.h"
#include "state.h"
#include "test.h"
#include "util.h"
#include "video.h"

#include <assert.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int
main(int argc, const char* argv[]) {
  size_t read_ret;
  uint8_t os_rom[k_bbc_rom_size];
  uint8_t load_rom[k_bbc_rom_size];
  int i;
  struct os_window_struct* p_window;
  struct os_poller_struct* p_poller;
  struct os_sound_struct* p_sound_driver;
  struct bbc_struct* p_bbc;
  struct keyboard_struct* p_keyboard;
  size_t window_handle;
  size_t bbc_handle;
  uint32_t run_result;

  const char* rom_names[k_bbc_num_roms] = {};
  int sideways_ram[k_bbc_num_roms] = {};
  uint8_t disc_buffers[2][k_bbc_max_dsd_disc_size] = {};
  const char* disc_names[2] = {};
  struct util_file_map* p_disc_maps[2] = {};

  const char* os_rom_name = "roms/os12.rom";
  const char* load_name = NULL;
  const char* capture_name = NULL;
  const char* replay_name = NULL;
  const char* opt_flags = "";
  const char* log_flags = "";
  int debug_flag = 0;
  int run_flag = 0;
  int print_flag = 0;
  int fast_flag = 0;
  int test_flag = 0;
  int accurate_flag = 0;
  int disc_writeable_flag = 0;
  int disc_mutable_flag = 0;
  int debug_stop_addr = 0;
  int pc = 0;
  int mode = k_cpu_mode_interp;
  uint64_t cycles = 0;
  uint32_t expect = 0;

  rom_names[k_bbc_default_dfs_rom_slot] = "roms/DFS-0.9.rom";
  rom_names[k_bbc_default_lang_rom_slot] = "roms/basic.rom";

  for (i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (i + 2 < argc) {
      const char* val1 = argv[i + 1];
      const char* val2 = argv[i + 2];
      if (!strcmp(arg, "-rom")) {
        int bank;
        (void) sscanf(val1, "%x", &bank);
        if (bank < 0 || bank >= k_bbc_num_roms) {
          errx(1, "ROM bank number out of range");
        }
        rom_names[bank] = val2;
        i += 2;
      }
    }
    if (i + 1 < argc) {
      const char* val = argv[i + 1];
      if (!strcmp(arg, "-os")) {
        os_rom_name = val;
        ++i;
      } else if (!strcmp(arg, "-load")) {
        load_name = val;
        ++i;
      } else if (!strcmp(arg, "-capture")) {
        capture_name = val;
        ++i;
      } else if (!strcmp(arg, "-replay")) {
        replay_name = val;
        ++i;
      } else if (!strcmp(arg, "-disc")) {
        disc_names[0] = val;
        ++i;
      } else if (!strcmp(arg, "-disc1")) {
        disc_names[1] = val;
        ++i;
      } else if (!strcmp(arg, "-opt")) {
        opt_flags = val;
        ++i;
      } else if (!strcmp(arg, "-log")) {
        log_flags = val;
        ++i;
      } else if (!strcmp(arg, "-stopat")) {
        (void) sscanf(val, "%x", &debug_stop_addr);
        ++i;
      } else if (!strcmp(arg, "-pc")) {
        (void) sscanf(val, "%x", &pc);
        ++i;
      } else if (!strcmp(arg, "-mode")) {
        if (!strcmp(val, "jit")) {
          mode = k_cpu_mode_jit;
        } else if (!strcmp(val, "interp")) {
          mode = k_cpu_mode_interp;
        } else if (!strcmp(val, "inturbo")) {
          mode = k_cpu_mode_inturbo;
        } else {
          errx(1, "unknown mode");
        }
        ++i;
      } else if (!strcmp(arg, "-swram")) {
        int bank;
        (void) sscanf(val, "%x", &bank);
        if (bank < 0 || bank >= k_bbc_num_roms) {
          errx(1, "RAM bank number out of range");
        }
        sideways_ram[bank] = 1;
        ++i;
      } else if (!strcmp(arg, "-cycles")) {
        (void) sscanf(val, "%ld", &cycles);
        ++i;
      } else if (!strcmp(arg, "-expect")) {
        (void) sscanf(val, "%x", &expect);
        ++i;
      }
    }
    if (!strcmp(arg, "-d")) {
      debug_flag = 1;
    } else if (!strcmp(arg, "-r")) {
      run_flag = 1;
    } else if (!strcmp(arg, "-p")) {
      print_flag = 1;
    } else if (!strcmp(arg, "-f")) {
      fast_flag = 1;
    } else if (!strcmp(arg, "-t")) {
      test_flag = 1;
    } else if (!strcmp(arg, "-a")) {
      accurate_flag = 1;
    } else if (!strcmp(arg, "-w")) {
      disc_writeable_flag = 1;
    } else if (!strcmp(arg, "-m")) {
      disc_mutable_flag = 1;
    }
  }

  (void) memset(os_rom, '\0', k_bbc_rom_size);
  (void) memset(load_rom, '\0', k_bbc_rom_size);

  read_ret = util_file_read_fully(os_rom, k_bbc_rom_size, os_rom_name);
  if (read_ret != k_bbc_rom_size) {
    errx(1, "can't load OS rom");
  }

  if (test_flag) {
    mode = k_cpu_mode_jit;
  }

  p_bbc = bbc_create(mode,
                     os_rom,
                     debug_flag,
                     run_flag,
                     print_flag,
                     fast_flag,
                     accurate_flag,
                     opt_flags,
                     log_flags,
                     debug_stop_addr);
  if (p_bbc == NULL) {
    errx(1, "bbc_create failed");
  }

/* TODO: re-enable when JIT is re-written.
  if (test_flag) {
    test_do_tests(p_bbc);
    return 0;
  }
*/

  if (pc != 0) {
    bbc_set_pc(p_bbc, pc);
  }
  if (cycles != 0) {
    bbc_set_stop_cycles(p_bbc, cycles);
  }

  for (i = 0; i < k_bbc_num_roms; ++i) {
    const char* p_rom_name = rom_names[i];
    if (p_rom_name != NULL) {
      (void) memset(load_rom, '\0', k_bbc_rom_size);
      (void) util_file_read_fully(load_rom, k_bbc_rom_size, p_rom_name);
      bbc_load_rom(p_bbc, i, load_rom);
    }
    if (sideways_ram[i]) {
      bbc_make_sideways_ram(p_bbc, i);
    }
  }

  if (load_name != NULL) {
    state_load(p_bbc, load_name);
  }

  /* Load the disc into the drive! */
  for (i = 0; i <= 1; ++i) {
    uint8_t* p_data;
    size_t buffer_size;
    size_t buffer_filled;

    int is_dsd = 0;
    size_t max_size = k_bbc_max_dsd_disc_size;
    const char* disc_name = disc_names[i];
    struct util_file_map* p_disc_map = NULL;

    if (disc_name == NULL) {
      continue;
    }
    if (strstr(disc_name, ".dsd") != NULL) {
      is_dsd = 1;
    }
    if (disc_mutable_flag) {
      p_disc_map = util_file_map(disc_name, max_size, 1);
      buffer_size = util_file_map_get_size(p_disc_map);
      buffer_filled = buffer_size;
      p_data = util_file_map_get_ptr(p_disc_map);
      p_disc_maps[i] = p_disc_map;
    } else {
      if (is_dsd) {
        buffer_size = k_bbc_max_dsd_disc_size;
      } else {
        buffer_size = k_bbc_max_ssd_disc_size;
      }
      p_data = disc_buffers[i];
      buffer_filled = util_file_read_fully(p_data, max_size, disc_name);
    }
    bbc_load_disc(p_bbc,
                  i,
                  p_data,
                  buffer_size,
                  buffer_filled,
                  is_dsd,
                  disc_writeable_flag);
  }

  /* Set up keyboard capture / replay. */
  p_keyboard = bbc_get_keyboard(p_bbc);
  if (capture_name) {
    keyboard_set_capture_file_name(p_keyboard, capture_name);
  }
  if (replay_name) {
    keyboard_set_replay_file_name(p_keyboard, replay_name);
  }

  p_window = os_window_create(p_keyboard,
                              bbc_get_video(p_bbc),
                              k_bbc_mode7_width,
                              k_bbc_mode7_height);
  if (p_window == NULL) {
    errx(1, "os_window_create failed");
  }

  p_poller = os_poller_create();
  if (p_poller == NULL) {
    errx(1, "os_poller_create failed");
  }

  p_sound_driver = NULL;
  if (!util_has_option(opt_flags, "sound:off")) {
    int ret;
    char* p_device_name = NULL;
    uint32_t sound_sample_rate = 0;
    uint32_t sound_buffer_size = 0;
    (void) util_get_u32_option(&sound_sample_rate, opt_flags, "sound:rate=");
    (void) util_get_u32_option(&sound_buffer_size, opt_flags, "sound:buffer=");
    (void) util_get_str_option(&p_device_name, opt_flags, "sound:dev=");
    p_sound_driver = os_sound_create(p_device_name,
                                     sound_sample_rate,
                                     sound_buffer_size);
    ret = os_sound_init(p_sound_driver);
    if (ret == 0) {
      sound_set_driver(bbc_get_sound(p_bbc), p_sound_driver);
    }
  }

  bbc_run_async(p_bbc);

  window_handle = os_window_get_handle(p_window);
  os_poller_add_handle(p_poller, window_handle);
  bbc_handle = bbc_get_client_handle(p_bbc);
  os_poller_add_handle(p_poller, bbc_handle);

  while (1) {
    char message;

    os_poller_poll(p_poller);

    if (os_poller_handle_triggered(p_poller, 0)) {
      os_window_process_events(p_window);
    }
    if (os_poller_handle_triggered(p_poller, 1)) {
      message = bbc_client_receive_message(p_bbc);
      if (message == k_message_exited) {
        break;
      } else {
        assert(message == k_message_vsync);
        os_window_render(p_window);
        if (bbc_get_vsync_wait_for_render(p_bbc)) {
          bbc_client_send_message(p_bbc, k_message_render_done);
        }
      }
    }
  }

  run_result = bbc_get_run_result(p_bbc);
  if (expect) {
    if (run_result != expect) {
      errx(1, "run result %x is not as expected", run_result);
    }
  }

  os_poller_destroy(p_poller);
  os_window_destroy(p_window);
  bbc_destroy(p_bbc);

  if (p_sound_driver != NULL) {
    os_sound_destroy(p_sound_driver);
  }

  for (i = 0; i <= 1; ++i) {
    struct util_file_map* p_disc_map = p_disc_maps[i];
    if (p_disc_map != NULL) {
      util_file_unmap(p_disc_map);
    }
  }

  return 0;
}
