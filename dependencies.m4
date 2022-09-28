dnl This is the list of distro packages which are required by
dnl virt-p2v.
dnl
dnl This file is processed by m4 with only one of the following
dnl symbols defined (depending on the target distro):
dnl
dnl   REDHAT=1       Fedora, RHEL, CentOS, SL and workalikes
dnl   DEBIAN=1       Debian and Ubuntu
dnl   ARCHLINUX=1    Arch Linux
dnl   SUSE=1         SUSE, OpenSUSE
dnl   OPENMANDRIVA=1 OpenMandriva
dnl
dnl NB 1: Must be one package name per line.  Blank lines are ignored.
dnl
dnl NB 2: This works differently from appliance/packagelist.in
dnl because we don't care about the current DISTRO (the one on
dnl which libguestfs is being compiled), since we can "cross-build"
dnl the virt-p2v ISO to any other Linux distro.
dnl
dnl NB 3: libguestfs is not a dependency of virt-p2v.  libguestfs
dnl only runs on the virt-v2v conversion server.

ifelse(REDHAT,1,
  dnl Used by the virt-p2v binary.
  pcre2
  libxml2
  gtk3
  dbus-libs

  dnl Run as external programs by the p2v binary.
  /usr/bin/ssh
  nbdkit-server
  nbdkit-file-plugin
  which

  dnl Generally useful tools to use within xterm
  vim-minimal

  dnl Useful disk and diagnostic utilities.
  iscsi-initiator-utils

  dnl X11 environment
  /usr/bin/xinit
  /usr/bin/Xorg
  xorg-x11-drivers
  xorg-x11-fonts-Type1
  dejavu-sans-fonts
  dejavu-sans-mono-fonts
  mesa-dri-drivers
  metacity

  NetworkManager
  nm-connection-editor
  network-manager-applet
  dnl dbus is required by nm-applet, but not a dependency in Fedora
  dbus-x11
  dnl sysadmins prefer ifconfig
  net-tools

  dnl RHBZ#1157679
  @hardware-support

  dnl UEFI Boot (RHBZ#2038105)
  shim-x64
  grub2-efi-x64-cdboot
)

ifelse(DEBIAN,1,
  libpcre2-8-0
  libxml2
  libgtk-3-0
  libdbus-1-3
  openssh-client
  nbdkit
  debianutils
  vim-tiny
  open-iscsi
  xorg
  xserver-xorg-video-all
  fonts-dejavu
  metacity
  network-manager
  network-manager-gnome
  dbus-x11
  net-tools
)

ifelse(ARCHLINUX,1,
  pcre2
  libxml2
  gtk3
  dbus
  openssh
  nbdkit
  which
  vim-tiny
  open-iscsi
  xorg-xinit
  xorg-server
  xf86-video-*
  ttf-dejavu
  metacity
  NetworkManager
  nm-connection-editor
  network-manager-applet
  dbus-x11
  net-tools
)

ifelse(SUSE,1,
  pcre2
  libxml2
  gtk3
  libdbus-1-3
  nbdkit-server
  nbdkit-file-plugin
  openssh
  dnl /usr/bin/which is in util-linux on SUSE
  vim
  open-iscsi
  xinit
  xorg-x11-server
  xf86-video-*
  dejavu-fonts
  NetworkManager
  xf86-input-*
  icewm-lite
  dbus-1-x11
  yast2-network
  libyui-qt
  SuSEfirewall2
)

ifelse(OPENMANDRIVA,1,
  dnl Used by the virt-p2v binary.
  pcre2
  libxml2
  gtk3
  dbus-libs

  dnl Run as external programs by the p2v binary.
  /usr/bin/ssh
  nbdkit-server
  nbdkit-file-plugin
  which

  dnl Generally useful tools to use within xterm
  vim-enhanced

  dnl X11 environment
  /usr/bin/xinit
  /usr/bin/Xorg
  xorg-x11-drivers
  xorg-x11-fonts-Type1
  dejavu-sans-fonts
  dejavu-sans-mono-fonts
  mesa-dri-drivers
  kwin_x11

  NetworkManager
  nm-connection-editor
  network-manager-applet
  dnl dbus is required by nm-applet, but not a dependency in Fedora
  dbus-x11
  dnl sysadmins prefer ifconfig
  net-tools
)

dnl Run as external programs by the p2v binary.
curl
ethtool
gawk
lsscsi
pciutils
usbutils
util-linux
xterm

dnl Generally useful tools to use within xterm
less

dnl The hwdata package contains PCI IDs, used by virt-p2v to display
dnl network vendor information (RHBZ#855059).
hwdata

dnl Useful disk and diagnostic utilities.
hdparm
smartmontools
