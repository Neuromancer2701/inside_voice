FROM ghcr.io/zephyrproject-rtos/zephyr-build:v0.26-branch

ARG ZEPHYR_VERSION=v3.7-branch

WORKDIR /workdir

# Initialize Zephyr workspace and fetch with shallow clones
RUN west init -m https://github.com/zephyrproject-rtos/zephyr --mr ${ZEPHYR_VERSION} && \
    west update --narrow -o=--depth=1 && \
    west zephyr-export

# Install Zephyr Python requirements
RUN pip3 install -r /workdir/zephyr/scripts/requirements.txt

# Project source will be mounted here
WORKDIR /workdir/project
