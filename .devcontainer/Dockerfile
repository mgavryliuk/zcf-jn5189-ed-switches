FROM ubuntu:latest
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/

ENV VENV_DIR=/venv
ENV TOOLCHAIN_DIR=/opt/arm-none-eabi

ARG TOOLCHAIN_ARCH=x86_64
ARG TOOLCHAIN_VERSION=14.2.rel1

ENV TOOLCHAIN_URL="https://developer.arm.com/-/media/Files/downloads/gnu/${TOOLCHAIN_VERSION}/binrel/arm-gnu-toolchain-${TOOLCHAIN_VERSION}-${TOOLCHAIN_ARCH}-arm-none-eabi.tar.xz"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    wget \
    xz-utils \
    make \
    cmake \
    clangd \
    clang-format \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p ${TOOLCHAIN_DIR} && \
    wget -qO- ${TOOLCHAIN_URL} | tar xJ --strip-components=1 -C ${TOOLCHAIN_DIR}

RUN uv python install 3.14 && uv venv -p 3.14 ${VENV_DIR} && uv --directory ${VENV_DIR} pip install lxml
ENV PATH=${TOOLCHAIN_DIR}/bin:${PATH}

CMD ["bash"]
