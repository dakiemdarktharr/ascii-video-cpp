# syntax=docker/dockerfile:1
FROM ubuntu:24.04 AS build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build qt6-base-dev libopencv-dev ffmpeg fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=ON -DCMAKE_INSTALL_PREFIX=/usr \
    && cmake --build /build --parallel 2

FROM build AS test
ENV QT_QPA_PLATFORM=offscreen
RUN ctest --test-dir /build --output-on-failure
ENTRYPOINT ["ctest", "--test-dir", "/build", "--output-on-failure"]

FROM ubuntu:24.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive QT_QPA_PLATFORM=offscreen
RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt6widgets6 qt6-qpa-plugins libopencv-core406t64 libopencv-imgproc406t64 \
    libopencv-imgcodecs406t64 libopencv-videoio406t64 ffmpeg fonts-dejavu-core \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 app
COPY --from=build /build/ascii-video-cpp /usr/local/bin/ascii-video-cpp
USER app
WORKDIR /data
ENTRYPOINT ["ascii-video-cpp"]
CMD ["--help"]

FROM runtime AS gui
USER root
RUN apt-get update && apt-get install -y --no-install-recommends \
    xvfb x11vnc novnc websockify openbox tini \
    && rm -rf /var/lib/apt/lists/*
COPY docker/start-gui.sh /usr/local/bin/start-gui
RUN chmod +x /usr/local/bin/start-gui
ENV QT_QPA_PLATFORM=xcb DISPLAY=:99
USER app
EXPOSE 6080
ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/start-gui"]
CMD []
