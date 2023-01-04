FROM ubuntu:jammy-20220130
ENV DEBIAN_FRONTEND noninteractive
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections
RUN apt-get update
RUN apt-get install -y systemd sudo apt-utils ssh
ENV MAGAOX_ROLE container
RUN echo "MAGAOX_ROLE=${MAGAOX_ROLE}" > /etc/profile.d/magaox_role.sh
ADD ./setup/_common.sh /setup/
ADD ./setup/setup_users_and_groups.sh /setup/
RUN bash /setup/setup_users_and_groups.sh
ADD ./setup/steps/install_ubuntu_22.04_packages.sh /setup/steps/
RUN bash /setup/steps/install_ubuntu_22.04_packages.sh
ADD ./setup/steps/configure_ubuntu_22.04.sh /setup/steps/
RUN bash /setup/steps/configure_ubuntu_22.04.sh
ADD . /opt/MagAOX/source/MagAOX
WORKDIR /opt/MagAOX/source/MagAOX/setup
# RUN bash setup_users_and_groups.sh
# RUN bash provision.sh
ENV DEBIAN_FRONTEND dialog
RUN echo 'debconf debconf/frontend select Dialog' | debconf-set-selections
# USER xsup