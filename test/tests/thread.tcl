# [thread] command testing.

test thread-1.1 {spawn/await basic value} {
    set id [thread spawn {expr {20 + 22}}]
    thread await $id
} -ok {42}

test thread-1.2 {await propagates worker error} {
    set id [thread spawn {expr {1 / 0}}]
    catch {thread await $id} msg
    set msg
} -ok {divide by zero}

test thread-1.3 {ids + ready + await} {
    set id [thread spawn {expr {7}}]
    set ids [thread ids]
    set has [expr {$id in $ids}]
    set ready [thread ready $id]
    set value [thread await $id]
    list $has [expr {$ready == 0 || $ready == 1}] $value
} -ok {1 1 7}

test thread-1.4 {await twice is an error} {
    set id [thread spawn {expr {1}}]
    thread await $id
    set code [catch {thread await $id} msg]
    list $code [string first {unknown thread task "} $msg]
} -ok {1 0}

test thread-1.5 {worker uses isolated interpreter state} {
    set x 99
    set id [thread spawn {catch {set x} msg; set msg}]
    set worker [thread await $id]
    list [string first {can't read "x": no such variable} $worker] $x
} -ok {0 99}

test thread-2.1 {channel send from main, recv in worker} {
    set ch [thread channel create]
    set id [thread spawn [list thread channel recv $ch]]
    thread channel send $ch hello
    thread await $id
} -ok {hello}

test thread-2.2 {channel send from worker, recv in main} {
    set ch [thread channel create]
    set id [thread spawn [list thread channel send $ch world]]
    set value [thread channel recv $ch]
    thread await $id
    set value
} -ok {world}

test thread-2.3 {channel fifo ordering} {
    set ch [thread channel create]
    thread channel send $ch one
    thread channel send $ch two
    list [thread channel recv $ch] [thread channel recv $ch]
} -ok {one two}

test thread-2.4 {channel unknown id} {
    set code [catch {thread channel recv 999999} msg]
    list $code [string first {unknown thread channel "} $msg]
} -ok {1 0}
