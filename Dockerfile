FROM ubuntu
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get -y install gcc
RUN apt-get -y install libasound2-dev
RUN apt-get -y install libpulse-dev
RUN apt-get -y install libx11-dev
RUN apt-get -y install libxext-dev
RUN apt-get -y install xvfb
COPY . /src
WORKDIR /src
RUN xvfb-run ./build.sh
# -0 test/perf/clocksp.ssd
ENTRYPOINT [ "./beebjit", "-fast", "-opt", "sound:off,bbc:cycles-per-run=10000000","-terminal","-headless" ]
