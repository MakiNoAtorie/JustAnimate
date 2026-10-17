#!/bin/bash
# PreToolUse guard: a process kill by name or pattern also ends other worktrees' builds and test suites.
input=$(cat)
cmd=$(printf '%s' "$input" | tr -d '\r')
deny() {
    echo "Blocked: $1. Other worktrees run the same processes. Stop only a process you started, by PID, after checking its path: Get-Process | Where-Object Path -eq '<full path>' then Stop-Process -Id <pid> (Bash: kill <pid>)." >&2
    exit 2
}
printf '%s' "$cmd" | grep -Eiq '(^|[;&|(]|"command":")[[:space:]]*(sudo[[:space:]]+)?(pkill|killall)([^a-z-]|$)' && deny "pkill/killall match by name"
printf '%s' "$cmd" | grep -Eiq 'while[^;]*pgrep' && deny "a pgrep wait loop matches other sessions' processes"
printf '%s' "$cmd" | grep -Eiq 'taskkill[^|;&]*/(im|fi)' && deny "taskkill /IM or /FI matches by name"
printf '%s' "$cmd" | grep -Eiq 'stop-process[^|;&]*-(name|processname)' && deny "Stop-Process -Name matches by name"
printf '%s' "$cmd" | grep -Eiq '\|[[:space:]]*(xargs[[:space:]]+)?(stop-process|kill)([^a-z-]|$)' && deny "a piped kill stops every matched process"
exit 0
