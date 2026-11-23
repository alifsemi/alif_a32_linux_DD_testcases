#!/bin/sh
#
# cpu_hotplug_test.sh - functional + stress test for Linux CPU hotplug
#
# Drives the same sysfs interface as:
#   echo 0 > /sys/devices/system/cpu/cpu1/online    # offline cpu1
#   echo 1 > /sys/devices/system/cpu/cpu1/online    # online cpu1
#
# Usage: ./cpu_hotplug_test.sh [-n iterations] [-c "cpu list"] [-v]
#   -n  stress iterations per CPU                   (default 50)
#   -c  space separated CPU numbers to test         (default: every CPU with an 'online' file)
#   -v  keep the kernel console loglevel unchanged. By default pr_info lines such as
#       "CPU1 killed." are hidden from the console while the test runs (they still
#       reach dmesg) and the original loglevel is restored on exit.
#
# Needs root. Only POSIX sh builtins plus: cat, grep, sleep, dmesg, id.
# (No awk, getopts, taskset, sed, cut, tr, ls or wc.) Optional: date, tail.
# Exit status: 0 = all pass, 1 = at least one failure, 2 = setup problem.

SYS=${SYS:-/sys/devices/system/cpu}
ITER=50
CPUS=""
VERBOSE=0
PASS=0; FAIL=0; SKIP=0
BG_PIDS=""
ORIG=""
PK_SAVED=""
REASON=""
MARK_TXT="cpu_hotplug_test: begin $$"
USE_MARK=0
DMESG_MARK=0

usage() {
	echo "Usage: $0 [-n iterations] [-c \"cpu list\"] [-v]"
	exit 2
}

log()  { echo "[INFO] $*"; }
pass() { PASS=$((PASS + 1)); echo "[PASS] $*"; }
fail() { FAIL=$((FAIL + 1)); echo "[FAIL] $*"; }
skip() { SKIP=$((SKIP + 1)); echo "[SKIP] $*"; }

# have TOOL -> true if an executable TOOL is on PATH. Plain PATH search: the
# 'command' and 'type' builtins are not available in every BusyBox ash build.
have() {
	_hold=$IFS
	IFS=:
	for _hd in $PATH; do
		IFS=$_hold
		[ -n "$_hd" ] && [ -x "$_hd/$1" ] && return 0
	done
	IFS=$_hold
	return 1
}
rd()         { cat "$1" 2>/dev/null; }
cpu_state()  { rd "$SYS/cpu$1/online"; }
set_cpu()    { ( echo "$2" > "$SYS/cpu$1/online" ) 2>/dev/null; }
online_cnt() { grep -c '^processor' /proc/cpuinfo; }
in_cpuinfo() { grep -Eq "^processor[[:space:]]*:[[:space:]]*$1\$" /proc/cpuinfo; }
in_stat()    { grep -q "^cpu$1 " /proc/stat; }
now()        { if have date; then date +%s; else echo 0; fi; }
tail_n()     { if have tail; then tail -n "$1"; else cat; fi; }

count_cpus() {
	_cnt=0
	for _d in "$SYS"/cpu[0-9]*; do
		[ -d "$_d" ] && _cnt=$((_cnt + 1))
	done
	echo "$_cnt"
}

# in_list "0,2-3" 2  -> exit 0 if CPU 2 is in the kernel cpulist
in_list() {
	_n=$2
	_oldifs=$IFS
	IFS=,
	for _r in $1; do
		IFS=$_oldifs
		case $_r in
		"")  continue ;;
		*-*) _lo=${_r%-*}; _hi=${_r#*-} ;;
		*)   _lo=$_r; _hi=$_r ;;
		esac
		if [ "$_n" -ge "$_lo" ] && [ "$_n" -le "$_hi" ]; then
			return 0
		fi
	done
	IFS=$_oldifs
	return 1
}

# task_cpu PID -> CPU the task last ran on (field 39 of /proc/PID/stat)
task_cpu() {
	_line=""
	{ read -r _line < "/proc/$1/stat"; } 2>/dev/null || return 1
	_rest=${_line##*) }
	set -- $_rest
	[ $# -ge 37 ] || return 1
	shift 36
	echo "$1"
}

# cpu_busy N -> user+nice+system ticks of cpuN from /proc/stat (0 if offline)
cpu_busy() {
	set -- $(grep "^cpu$1 " /proc/stat)
	echo $(( ${2:-0} + ${3:-0} + ${4:-0} ))
}

# cpu_runs_work N -> true if cpuN burned CPU time during the next second
cpu_runs_work() {
	_a=$(cpu_busy "$1"); sleep 1; _b=$(cpu_busy "$1")
	[ $((_b - _a)) -gt 10 ]
}

spawn_load() {
	_i=0
	while [ "$_i" -lt "$1" ]; do
		sh -c 'while :; do :; done' &
		BG_PIDS="$BG_PIDS $!"
		_i=$((_i + 1))
	done
}

kill_bg() {
	for _p in $BG_PIDS; do
		kill "$_p" 2>/dev/null
		wait "$_p" 2>/dev/null
	done
	BG_PIDS=""
}

# ---- kernel log handling -------------------------------------------------
mark_log() {
	if ( echo "<6>$MARK_TXT" > /dev/kmsg ) 2>/dev/null; then
		USE_MARK=1
	else
		USE_MARK=0
		DMESG_MARK=$(dmesg 2>/dev/null | grep -c '')
	fi
}

# print every kernel log line written since mark_log
scan_dmesg() {
	if [ "$USE_MARK" = 1 ]; then
		_seen=0
		dmesg 2>/dev/null | while IFS= read -r _l; do
			if [ "$_seen" = 1 ]; then
				echo "$_l"
			else
				case $_l in *"$MARK_TXT"*) _seen=1 ;; esac
			fi
		done
	else
		_ln=0
		dmesg 2>/dev/null | while IFS= read -r _l; do
			_ln=$((_ln + 1))
			[ "$_ln" -gt "$DMESG_MARK" ] && echo "$_l"
		done
	fi
}

show_log() { scan_dmesg | tail_n 8; }

quiet_console() {
	[ "$VERBOSE" = 1 ] && return
	[ -w /proc/sys/kernel/printk ] || return
	read -r _p1 _p2 _p3 _p4 < /proc/sys/kernel/printk
	PK_SAVED="$_p1 $_p2 $_p3 $_p4"
	# console_loglevel 6 prints only messages more severe than pr_info
	[ "$_p1" -gt 6 ] && echo "6 $_p2 $_p3 $_p4" > /proc/sys/kernel/printk
}

cleanup() {
	kill_bg
	for _e in $ORIG; do
		set_cpu "${_e%%=*}" "${_e##*=}"
	done
	[ -n "$PK_SAVED" ] && echo "$PK_SAVED" > /proc/sys/kernel/printk 2>/dev/null
}
trap cleanup EXIT INT TERM

# check_state CPU WANT   (WANT: 0 = offline, 1 = online)
# Every observer must agree. REASON lists the ones that do not. Returns non-zero on mismatch.
check_state() {
	_c=$1; _want=$2; REASON=""
	_on=$(rd "$SYS/online"); _off=$(rd "$SYS/offline")
	[ "$(cpu_state "$_c")" = "$_want" ] || REASON="$REASON sysfs online=$(cpu_state "$_c");"
	if [ "$_want" = 0 ]; then
		in_list "$_on"  "$_c" && REASON="$REASON still in $SYS/online;"
		in_list "$_off" "$_c" || REASON="$REASON missing from $SYS/offline;"
		in_cpuinfo "$_c"      && REASON="$REASON still in /proc/cpuinfo;"
		in_stat "$_c"         && REASON="$REASON still in /proc/stat;"
	else
		in_list "$_on"  "$_c" || REASON="$REASON missing from $SYS/online;"
		in_list "$_off" "$_c" && REASON="$REASON still in $SYS/offline;"
		in_cpuinfo "$_c"      || REASON="$REASON missing from /proc/cpuinfo;"
		in_stat "$_c"         || REASON="$REASON missing from /proc/stat;"
	fi
	[ -z "$REASON" ]
}

# ---------------------------------------------------------------- T1
test_basic() {
	log "T1: basic offline -> online, cross-checked against sysfs masks, /proc/cpuinfo and /proc/stat"
	for c in $CPUS; do
		set_cpu "$c" 1
		before=$(online_cnt)

		if ! set_cpu "$c" 0; then
			fail "cpu$c: offline write rejected"; show_log; continue
		fi
		if check_state "$c" 0 && [ "$(online_cnt)" -eq $((before - 1)) ]; then
			pass "cpu$c offline: sysfs, masks, /proc/cpuinfo and /proc/stat agree"
		else
			fail "cpu$c offline:$REASON online count $(online_cnt), expected $((before - 1))"
		fi

		if ! set_cpu "$c" 1; then
			fail "cpu$c: online write rejected"; show_log; continue
		fi
		if check_state "$c" 1 && [ "$(online_cnt)" -eq "$before" ]; then
			pass "cpu$c online: sysfs, masks, /proc/cpuinfo and /proc/stat agree"
		else
			fail "cpu$c online:$REASON online count $(online_cnt), expected $before"
		fi
	done
}

# ---------------------------------------------------------------- T2
test_idempotent() {
	log "T2: repeated writes of the same value must be harmless"
	for c in $CPUS; do
		set_cpu "$c" 0
		if set_cpu "$c" 0 && [ "$(cpu_state "$c")" = 0 ]; then
			pass "cpu$c: second offline write ok"
		else
			fail "cpu$c: second offline write failed / state wrong"
		fi
		set_cpu "$c" 1
		if set_cpu "$c" 1 && [ "$(cpu_state "$c")" = 1 ]; then
			pass "cpu$c: second online write ok"
		else
			fail "cpu$c: second online write failed / state wrong"
		fi
	done
}

# ---------------------------------------------------------------- T3
# Without taskset we cannot pin a task, so oversubscribe the system with busy loops
# and watch the ones the scheduler happened to place on the CPU we are about to offline.
test_migration() {
	log "T3: tasks running on a CPU are migrated off it when it goes offline"
	ncpu=$(count_cpus)
	spawn_load $((ncpu * 3))
	sleep 2
	for c in $CPUS; do
		set_cpu "$c" 1
		sleep 2
		victims=""
		for p in $BG_PIDS; do
			[ "$(task_cpu "$p")" = "$c" ] && victims="$victims $p"
		done
		if [ -z "$victims" ]; then
			skip "cpu$c: no load task happened to be running on it"; continue
		fi

		if ! set_cpu "$c" 0; then
			fail "cpu$c: offline write rejected with tasks running on it"; show_log; continue
		fi
		sleep 1
		dead=""; stuck=""; moved=0
		for p in $victims; do
			if ! kill -0 "$p" 2>/dev/null; then
				dead="$dead $p"
			elif [ "$(task_cpu "$p")" = "$c" ]; then
				stuck="$stuck $p"
			else
				moved=$((moved + 1))
			fi
		done
		if [ -n "$dead" ]; then
			fail "cpu$c: tasks died when the CPU went offline:$dead"
		elif [ -n "$stuck" ]; then
			fail "cpu$c: tasks still report running on the offline CPU:$stuck"
		else
			pass "cpu$c: $moved task(s) migrated off the CPU and kept running"
		fi
		set_cpu "$c" 1
	done
	kill_bg
}

# ---------------------------------------------------------------- T4
test_irq() {
	log "T4: no IRQ may be left with the offline CPU as its only target (best effort)"
	have_eff=0
	for f in /proc/irq/[0-9]*/effective_affinity_list; do
		[ -r "$f" ] && { have_eff=1; break; }
	done
	if [ "$have_eff" = 0 ]; then
		skip "effective_affinity_list not exposed by this kernel"; return
	fi
	for c in $CPUS; do
		set_cpu "$c" 0 || { fail "cpu$c: offline write rejected"; continue; }
		bad=""
		for f in /proc/irq/[0-9]*/effective_affinity_list; do
			v=""
			{ read -r v < "$f"; } 2>/dev/null
			[ "$v" = "$c" ] && bad="$bad ${f#/proc/irq/}"
		done
		if [ -z "$bad" ]; then
			pass "cpu$c: no IRQ effectively routed to the offline CPU"
		else
			fail "cpu$c: IRQs still routed to the offline CPU:$bad"
		fi
		set_cpu "$c" 1
	done
}

# ---------------------------------------------------------------- T5
test_stress() {
	log "T5: $ITER offline/online cycles per CPU while every CPU is busy"
	ncpu=$(count_cpus)
	spawn_load $((ncpu * 2))
	for c in $CPUS; do
		set_cpu "$c" 1
		n=0; bad=""; t0=$(now)
		while [ "$n" -lt "$ITER" ]; do
			n=$((n + 1))
			set_cpu "$c" 0              || { bad="iter $n: offline write failed"; break; }
			[ "$(cpu_state "$c")" = 0 ] || { bad="iter $n: still online after offline"; break; }
			set_cpu "$c" 1              || { bad="iter $n: online write failed"; break; }
			[ "$(cpu_state "$c")" = 1 ] || { bad="iter $n: still offline after online"; break; }
			# every 10th cycle (and the last) prove the CPU really executes work again
			if [ $((n % 10)) -eq 0 ] || [ "$n" -eq "$ITER" ]; then
				cpu_runs_work "$c" || { bad="iter $n: cpu$c is online but ran no work for 1s under load"; break; }
			fi
		done
		t1=$(now)
		if [ -z "$bad" ]; then
			pass "cpu$c: $ITER cycles clean in $((t1 - t0))s"
		else
			fail "cpu$c: $bad"; show_log
			set_cpu "$c" 1
		fi
	done
	kill_bg
}

# ---------------------------------------------------------------- T6
test_all_offline() {
	log "T6: offline every hotpluggable CPU, the last online CPU must survive"
	for c in $CPUS; do
		if [ "$(online_cnt)" -le 1 ]; then
			if set_cpu "$c" 0; then
				fail "cpu$c: kernel let the last online CPU go away"
			else
				pass "cpu$c: offlining the last online CPU refused"
			fi
		else
			set_cpu "$c" 0 || fail "cpu$c: offline write rejected"
		fi
	done
	if [ "$(online_cnt)" -ge 1 ]; then
		pass "$(online_cnt) CPU(s) still online with all hotpluggable CPUs offlined"
	else
		fail "no CPU left online"
	fi
	for c in $CPUS; do set_cpu "$c" 1; done
	if [ "$(online_cnt)" -eq "$(count_cpus)" ]; then
		pass "all CPUs back online ($(rd "$SYS/online"))"
	else
		fail "not every CPU came back online: $(rd "$SYS/online")"
	fi
}

# ---------------------------------------------------------------- T7
test_dmesg() {
	log "T7: kernel log scan for problems raised during the test"
	hits=$(scan_dmesg | grep -Ei 'BUG:|WARNING:|Oops|Unable to handle|Internal error|soft lockup|hard lockup|rcu.*stall|hung task|blocked for more than|failed to come online|failed to boot|CPU[0-9]+: failed')
	if [ -z "$hits" ]; then
		pass "no BUG/WARN/lockup/stall/hotplug-failure messages in dmesg"
	else
		fail "dmesg reported problems:"
		echo "$hits"
	fi
	info=$(scan_dmesg | grep -Ei 'killed|psci|Booted|secondary|CPU[0-9]+: (shutdown|thread)')
	if [ -n "$info" ]; then
		log "hotplug-related kernel messages: $(echo "$info" | grep -c ''), last ones:"
		echo "$info" | tail_n 3
	fi
}

# ---------------------------------------------------------------- main
while [ $# -gt 0 ]; do
	case $1 in
	-n) [ $# -ge 2 ] || usage; ITER=$2; shift 2 ;;
	-c) [ $# -ge 2 ] || usage; CPUS=$2; shift 2 ;;
	-v) VERBOSE=1; shift ;;
	*)  usage ;;
	esac
done
case $ITER in ''|*[!0-9]*) echo "-n needs a number"; exit 2 ;; esac

PATH="${PATH:+$PATH:}/bin:/sbin:/usr/bin:/usr/sbin"
export PATH
for t in cat grep sleep dmesg id; do
	have "$t" || { echo "missing required tool: $t (searched PATH=$PATH)"; exit 2; }
done
[ "$(id -u)" -eq 0 ] || { echo "Run as root"; exit 2; }
[ -d "$SYS" ]        || { echo "$SYS missing"; exit 2; }

if [ -z "$CPUS" ]; then
	for f in "$SYS"/cpu[0-9]*/online; do
		[ -e "$f" ] || continue
		d=${f%/online}
		CPUS="$CPUS ${d##*cpu}"
	done
fi
[ -n "$CPUS" ] || { echo "No CPU exposes an 'online' file (CONFIG_HOTPLUG_CPU disabled, or single CPU?)"; exit 2; }
for c in $CPUS; do
	[ -e "$SYS/cpu$c/online" ] || { echo "cpu$c has no 'online' file (boot CPU, or not present)"; exit 2; }
	ORIG="$ORIG $c=$(cpu_state "$c")"
done

read -r kver < /proc/sys/kernel/osrelease 2>/dev/null
log "kernel : ${kver:-unknown}"
log "online : $(rd "$SYS/online")   offline: $(rd "$SYS/offline")   testing:$CPUS   iterations: $ITER"

mark_log
quiet_console

test_basic
test_idempotent
test_migration
test_irq
test_stress
test_all_offline
test_dmesg

echo "=============================="
echo "PASS=$PASS FAIL=$FAIL SKIP=$SKIP"
cleanup
trap - EXIT INT TERM
[ "$FAIL" -eq 0 ] && exit 0
exit 1
