ARG BASE_IMAGE=ghcr.io/magao-x/magao-x-setup:cli
FROM ${BASE_IMAGE}
ENV MAGAOX_ROLE=container
WORKDIR /opt/MagAOX/source/magao-x-setup
USER root
RUN bash -lx steps/install_MagAOX.sh
USER xsup
