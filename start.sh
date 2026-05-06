#!/system/bin/sh

IMG="/data/media/0/archdroid/linux.img"
LOOP="/dev/block/loop0"
MNT="/mnt/archdroid"
LOG="/data/media/0/archdroid/archdroid.log"
CHROOT_ENV="HOME=/root PATH=/usr/bin:/usr/sbin:/usr/local/bin:/bin:/sbin DISPLAY=:0 XDG_RUNTIME_DIR=/tmp"
FB_DEV="/dev/graphics/fb0"
FB_HZ=60

ENABLE_LOG=1
START_DE=1

for arg in "$@"; do
  case "$arg" in
    --no-log) ENABLE_LOG=0 ;;
    --no-de)  START_DE=0 ;;
  esac
done

log() {
  local level="$1"; shift
  local msg="[$(date '+%Y-%m-%d %H:%M:%S')] [$level] $*"
  echo "$msg"
  [ "$ENABLE_LOG" -eq 1 ] && echo "$msg" >> "$LOG"
}

info()  { log "INFO " "$@"; }
ok()    { log "OK   " "$@"; }
warn()  { log "WARN " "$@"; }
err()   { log "ERROR" "$@"; exit 1; }

run() {
  local desc="$1"; shift
  info "$desc"
  if "$@"; then
    ok "$desc — done"
  else
    err "$desc — FAILED (exit $?)"
  fi
}

cleanup() {
  warn "Interrupted — resuming Android UI"
  resume_android_ui
  exit 1
}

mount_disk() {
  info "=== Mounting virtual disk ==="

  if losetup "$LOOP" 2>/dev/null | grep -q "$IMG"; then
    warn "Loop device already set up, skipping losetup"
  else
    run "Setting up loop device $LOOP -> $IMG" \
      losetup "$LOOP" "$IMG"
  fi

  if [ ! -d "$MNT" ]; then
    run "Creating mount point $MNT" mkdir -p "$MNT"
  else
    warn "Mount point $MNT already exists, skipping mkdir"
  fi

  if mountpoint -q "$MNT" 2>/dev/null; then
    warn "$MNT already mounted, skipping"
  else
    run "Mounting ext4 image at $MNT" \
      mount -t ext4 -o loop "$LOOP" "$MNT/"
  fi
}

mount_pseudo() {
  info "=== Mounting pseudo filesystems ==="

  _bind_mount() {
    local type="$1" src="$2" dst="$3"
    if mountpoint -q "$dst" 2>/dev/null; then
      warn "$dst already mounted, skipping"
    else
      run "Mounting $type -> $dst" mount -t "$type" "$src" "$dst"
    fi
  }

  _bind_mount tmpfs  tmpfs  "$MNT/run"
  _bind_mount proc   proc   "$MNT/proc"
  _bind_mount sysfs  sys    "$MNT/sys"

  if mountpoint -q "$MNT/dev" 2>/dev/null; then
    warn "$MNT/dev already bind-mounted, skipping"
  else
    run "Bind-mounting /dev -> $MNT/dev" \
      mount -o bind /dev "$MNT/dev"
  fi

  _bind_mount devpts devpts "$MNT/dev/pts"
  _bind_mount tmpfs  tmpfs  "$MNT/tmp"
}

wake_screen() {
  if dumpsys power | grep -q "mWakefulness=Awake"; then
    echo "Screen is already awake"
  else
    input keyevent 26
  fi
}

stop_android_ui() {
  info "=== Stopping Android UI ==="
  run "Waking screen" wake_screen
  sleep 1 
  run "Syncing filesystem buffers" sync
  run "Suspending system_server"   pkill -STOP system_server
  run "Suspending surfaceflinger"  pkill -STOP surfaceflinger
  ok "Android UI suspended"
}

resume_android_ui() {
  info "=== Resuming Android UI ==="
  run "Killing fb_refresh" killall fb_refresh
  run "Resuming surfaceflinger"  pkill -CONT surfaceflinger
  run "Resuming system_server"   pkill -CONT system_server
  ok "Android UI resumed"
}

enter_chroot_de() {
  info "=== Entering chroot with DE ==="
  trap cleanup INT TERM
  stop_android_ui

  if [ "$ENABLE_LOG" -eq 1 ]; then
    info "DE logs -> stdout/stderr (logging enabled)"
    chroot "$MNT" /usr/bin/env -i $CHROOT_ENV /bin/bash -c \
      "chmod 000 /dev/video*; mkdir -p /run/dbus && dbus-daemon --system --fork; fb_refresh $FB_DEV $FB_HZ & startx"
  else
    info "DE logs suppressed (--no-log mode)"
    chroot "$MNT" /usr/bin/env -i $CHROOT_ENV /bin/bash -c \
      "chmod 000 /dev/video*; mkdir -p /run/dbus && dbus-daemon --system --fork; fb_refresh $FB_DEV $FB_HZ > /dev/null 2>&1 & startx"
  fi

  resume_android_ui
  trap - INT TERM
}

enter_chroot_shell() {
  info "=== Entering chroot shell ==="
  chroot "$MNT" /usr/bin/env -i $CHROOT_ENV /bin/bash
}

main() {
  [ "$ENABLE_LOG" -eq 1 ] && info "Log file: $LOG"
  info "Starting archdroid (flags: ENABLE_LOG=$ENABLE_LOG START_DE=$START_DE)"

  mount_disk
  mount_pseudo

  if [ "$START_DE" -eq 1 ]; then
    enter_chroot_de
  else
    enter_chroot_shell
  fi

  info "=== archdroid session ended ==="
}

main
