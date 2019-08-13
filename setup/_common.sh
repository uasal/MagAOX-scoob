#!/bin/bash
function log_error() {
    echo -e "$(tput setaf 1 2>/dev/null)$1$(tput sgr0 2>/dev/null)"
}
function log_success() {
    echo -e "$(tput setaf 2 2>/dev/null)$1$(tput sgr0 2>/dev/null)"
}
function log_warn() {
    echo -e "$(tput setaf 3 2>/dev/null)$1$(tput sgr0 2>/dev/null)"
}
function log_info() {
    echo -e "$(tput setaf 4 2>/dev/null)$1$(tput sgr0 2>/dev/null)"
}

function _cached_fetch() {
  url=$1
  filename=$2
  dest=$PWD
  if [[ ! -e $dest/$filename ]]; then
    if [[ ! -e /opt/MagAOX/.cache/$filename ]]; then
      curl --silent --show-errors -A "Mozilla/5.0" -L $url > /opt/MagAOX/.cache/$filename
      log_info "Downloaded to /opt/MagAOX/.cache/$filename"
    fi
    cp /opt/MagAOX/.cache/$filename $dest/$filename
    log_info "Copied to $dest/$filename"
  fi
}

DEFAULT_PASSWORD="extremeAO!"

function creategroup() {
  if [[ ! $(getent group $1) ]]; then
    sudo groupadd $1
    echo "Added group $1"
  else
    echo "Group $1 exists"
  fi
}

function createuser() {
  if getent passwd $1 > /dev/null 2>&1; then
    log_info "User account $1 exists"
  else
    sudo useradd $1
    echo -e "$DEFAULT_PASSWORD\n$DEFAULT_PASSWORD" | sudo passwd $1
    log_success "Created user account $1 with default password $DEFAULT_PASSWORD"
  fi
  sudo usermod -g magaox $1
  log_info "Added user $1 to group magaox"
  sudo mkdir -p /home/$1/.ssh
  sudo touch /home/$1/.ssh/authorized_keys
  sudo chown -R $1:magaox /home/$1/.ssh
  sudo chmod -R u=rwx,g=,o= /home/$1/.ssh
  sudo chmod u=rw,g=,o= /home/$1/.ssh/authorized_keys
  log_info "Append an ecdsa or ed25519 key to /home/$1/.ssh/authorized_keys to enable SSH login"
}
# We work around the buggy devtoolset /bin/sudo wrapper in provision.sh, but
# that means we have to explicitly enable it ourselves.
# (This crap again: https://bugzilla.redhat.com/show_bug.cgi?id=1319936)
if [[ -e /opt/rh/devtoolset-7/enable ]]; then
    set +u; source /opt/rh/devtoolset-7/enable; set -u
fi
# root doesn't get /usr/local/bin on their path, so add it
# (why? https://serverfault.com/a/838552)
if [[ $PATH != "*/usr/local/bin*" ]]; then
    export PATH="/usr/local/bin:$PATH"
fi
