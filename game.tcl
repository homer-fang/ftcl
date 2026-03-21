# FTCL Maze Raiders (real-time)
# - Enemy movement is autonomous; it does not wait for player movement.
# - Input is real-time with getch -noblock (no Enter required).
#
# Controls:
#   W/A/S/D: move
#   F: fire in facing direction
#   Q: quit
#
# Run:
#   cd /mnt/d/ftcl/ftcl
#   ./build/test/test_ftcl_suite /mnt/d/ftcl/ftcl/game.tcl

proc sign_to {from to} {
    if {$to > $from} {
        return 1
    }
    if {$to < $from} {
        return -1
    }
    return 0
}

proc abs_i {x} {
    if {$x < 0} {
        return [expr {-$x}]
    }
    return $x
}

proc key_of {ch} {
    set s [string tolower $ch]
    return [string range $s 0 0]
}

proc wall_key {x y} {
    return "$x:$y"
}

proc is_wall_removed {x y} {
    global wall_holes
    set k [wall_key $x $y]
    if {[info exists wall_holes($k)]} {
        return 1
    }
    return 0
}

proc static_tile {x y w h} {
    set max_x [expr {$w - 1}]
    set max_y [expr {$h - 1}]

    if {$x == 0 || $y == 0 || $x == $max_x || $y == $max_y} {
        return "#"
    }

    # Vertical maze walls
    if {($x == 5 && $y >= 2 && $y <= 13 && $y != 8) ||
        ($x == 11 && $y >= 1 && $y <= 14 && $y != 4 && $y != 10) ||
        ($x == 17 && $y >= 2 && $y <= 15 && $y != 6 && $y != 12) ||
        ($x == 21 && $y >= 1 && $y <= 13 && $y != 9)} {
        return "#"
    }

    # Horizontal maze walls
    if {($y == 4 && $x >= 3 && $x <= 10 && $x != 5) ||
        ($y == 8 && $x >= 7 && $x <= 20 && $x != 11 && $x != 17) ||
        ($y == 12 && $x >= 2 && $x <= 18 && $x != 5 && $x != 11)} {
        return "#"
    }

    # Swamp tiles (slow movement)
    if {($x >= 2 && $x <= 4 && $y >= 2 && $y <= 5) ||
        ($x >= 13 && $x <= 16 && $y >= 10 && $y <= 13) ||
        ($x >= 22 && $x <= 24 && $y >= 3 && $y <= 5)} {
        return "~"
    }

    # Trap tiles (damage on contact)
    if {($x == 8 && $y == 2) || ($x == 14 && $y == 6) || ($x == 19 && $y == 14)} {
        return "^"
    }

    return "."
}

proc base_tile {x y w h} {
    set t [static_tile $x $y $w $h]
    if {$t eq "#" && [is_wall_removed $x $y] == 1} {
        return "."
    }
    return $t
}

proc break_wall {x y w h} {
    global wall_holes

    if {$x <= 0 || $y <= 0 || $x >= [expr {$w - 1}] || $y >= [expr {$h - 1}]} {
        return 0
    }

    if {[base_tile $x $y $w $h] ne "#"} {
        return 0
    }

    set wall_holes([wall_key $x $y]) 1
    return 1
}

proc walkable {x y w h} {
    if {$x < 0 || $y < 0 || $x >= $w || $y >= $h} {
        return 0
    }
    set t [base_tile $x $y $w $h]
    if {$t eq "#"} {
        return 0
    }
    return 1
}

proc try_step {x y dx dy w h} {
    set nx [expr {$x + $dx}]
    set ny [expr {$y + $dy}]
    if {[walkable $nx $ny $w $h] == 1} {
        return [list $nx $ny]
    }
    return [list $x $y]
}

proc clear_path {x1 y1 x2 y2 w h} {
    if {$x1 == $x2} {
        set step [sign_to $y1 $y2]
        set y [expr {$y1 + $step}]
        while {$y != $y2} {
            if {[base_tile $x1 $y $w $h] eq "#"} {
                return 0
            }
            set y [expr {$y + $step}]
        }
        return 1
    }

    if {$y1 == $y2} {
        set step [sign_to $x1 $x2]
        set x [expr {$x1 + $step}]
        while {$x != $x2} {
            if {[base_tile $x $y1 $w $h] eq "#"} {
                return 0
            }
            set x [expr {$x + $step}]
        }
        return 1
    }

    return 0
}

proc can_hit_dir {px py ex ey dir_x dir_y w h} {
    if {$dir_x == 0 && $dir_y == 0} {
        return 0
    }

    if {$dir_x != 0} {
        if {$py != $ey} {
            return 0
        }
        set d [expr {$ex - $px}]
        if {$dir_x > 0 && $d <= 0} {
            return 0
        }
        if {$dir_x < 0 && $d >= 0} {
            return 0
        }
        if {[abs_i $d] > 6} {
            return 0
        }
        return [clear_path $px $py $ex $ey $w $h]
    }

    if {$px != $ex} {
        return 0
    }
    set d [expr {$ey - $py}]
    if {$dir_y > 0 && $d <= 0} {
        return 0
    }
    if {$dir_y < 0 && $d >= 0} {
        return 0
    }
    if {[abs_i $d] > 6} {
        return 0
    }
    return [clear_path $px $py $ex $ey $w $h]
}

proc respawn_enemy {seed w h} {
    set max_x [expr {$w - 2}]
    set max_y [expr {$h - 2}]

    for {set i 0} {$i < 30} {incr i} {
        set x [expr {1 + (($seed * 7 + $i * 3) % $max_x)}]
        set y [expr {1 + (($seed * 11 + $i * 5) % $max_y)}]
        if {[walkable $x $y $w $h] == 1} {
            return [list $x $y]
        }
    }

    return [list 1 1]
}

proc apply_enemy_move {x y p_dx p_dy s_dx s_dy px py w h} {
    set p1 [try_step $x $y $p_dx $p_dy $w $h]
    set nx [lindex $p1 0]
    set ny [lindex $p1 1]
    if {$nx != $x || $ny != $y} {
        return $p1
    }

    set p2 [try_step $x $y $s_dx $s_dy $w $h]
    set nx [lindex $p2 0]
    set ny [lindex $p2 1]
    if {$nx != $x || $ny != $y} {
        return $p2
    }

    # If blocked, nudge toward player in a single axis.
    set tx [sign_to $x $px]
    set ty [sign_to $y $py]
    if {$tx != 0} {
        set p3 [try_step $x $y $tx 0 $w $h]
        set nx [lindex $p3 0]
        set ny [lindex $p3 1]
        if {$nx != $x || $ny != $y} {
            return $p3
        }
    }
    if {$ty != 0} {
        set p4 [try_step $x $y 0 $ty $w $h]
        set nx [lindex $p4 0]
        set ny [lindex $p4 1]
        if {$nx != $x || $ny != $y} {
            return $p4
        }
    }

    return [list $x $y]
}

proc point_in_points {points x y} {
    foreach p $points {
        if {[lindex $p 0] == $x && [lindex $p 1] == $y} {
            return 1
        }
    }
    return 0
}

proc draw_board {w h px py e1x e1y e2x e2y bullet_points gemx gemy goalx goaly} {
    set out ""

    for {set y 0} {$y < $h} {incr y} {
        set row ""
        for {set x 0} {$x < $w} {incr x} {
            set cell [base_tile $x $y $w $h]

            if {$x == $goalx && $y == $goaly} {
                set cell "G"
            }
            if {$x == $gemx && $y == $gemy} {
                set cell "@"
            }
            if {$x == $e1x && $y == $e1y} {
                set cell "1"
            }
            if {$x == $e2x && $y == $e2y} {
                if {$cell eq "1"} {
                    set cell "B"
                } else {
                    set cell "2"
                }
            }
            if {[point_in_points $bullet_points $x $y] == 1} {
                set cell "*"
            }
            if {$x == $px && $y == $py} {
                if {$cell eq "1" || $cell eq "2" || $cell eq "B"} {
                    set cell "X"
                } else {
                    set cell "P"
                }
            }

            append row $cell
        }
        append out $row "\n"
    }

    return $out
}

set width 27
set height 17
set max_frames 1000
set tick_ms 8
set enemy_every 3

set frame 0
set hp 6
set score 0

set player_x 2
set player_y [expr {$height - 3}]
set player_slow 0

set dir_x 1
set dir_y 0

set bullets {}
set bullet_max_life 8
set max_bullets 32

set e1x [expr {$width - 3}]
set e1y 2
set e2x [expr {$width - 4}]
set e2y [expr {$height - 3}]
set e1_slow 0
set e2_slow 0

set gem_x 3
set gem_y 3
set goal_x [expr {$width - 2}]
set goal_y [expr {$height - 2}]

set cmd1 [thread channel create]
set cmd2 [thread channel create]
set bus [thread channel create]

array set wall_holes {}

set ai1_script "
proc sign_to {from to} {
    if {\$to > \$from} { return 1 }
    if {\$to < \$from} { return -1 }
    return 0
}
set running 1
while {\$running} {
    set msg \[thread channel recv $cmd1\]
    set kind \[lindex \$msg 0\]
    if {\$kind eq \"tick\"} {
        set sx \[lindex \$msg 1\]
        set sy \[lindex \$msg 2\]
        set px \[lindex \$msg 3\]
        set py \[lindex \$msg 4\]
        set f \[lindex \$msg 5\]

        set dx \[sign_to \$sx \$px\]
        set dy \[sign_to \$sy \$py\]

        if {\$f % 3 == 0} {
            thread channel send $bus \[list move E1 \$dx 0 0 \$dy\]
        } else {
            thread channel send $bus \[list move E1 0 \$dy \$dx 0\]
        }
    } elseif {\$kind eq \"stop\"} {
        set running 0
    }
}
set done {E1 stopped}
"

set ai2_script "
proc sign_to {from to} {
    if {\$to > \$from} { return 1 }
    if {\$to < \$from} { return -1 }
    return 0
}
set running 1
while {\$running} {
    set msg \[thread channel recv $cmd2\]
    set kind \[lindex \$msg 0\]
    if {\$kind eq \"tick\"} {
        set sx \[lindex \$msg 1\]
        set sy \[lindex \$msg 2\]
        set px \[lindex \$msg 3\]
        set py \[lindex \$msg 4\]
        set f \[lindex \$msg 5\]

        set dx \[sign_to \$sx \$px\]
        set dy \[sign_to \$sy \$py\]
        if {\$f % 5 == 0} {
            set dx \[expr {-\$dx}\]
        }

        thread channel send $bus \[list move E2 0 \$dy \$dx 0\]
    } elseif {\$kind eq \"stop\"} {
        set running 0
    }
}
set done {E2 stopped}
"

set ai1 [thread spawn $ai1_script]
set ai2 [thread spawn $ai2_script]

set ansi_clear "\033\[2J"
set ansi_home "\033\[H"
set ansi_hide_cursor "\033\[?25l"
set ansi_show_cursor "\033\[?25h"

set running 1
set status "running"

puts "$ansi_clear$ansi_home$ansi_hide_cursor"

while {$running} {
    incr frame

    if {$player_slow > 0} {
        incr player_slow -1
    }

    set key_raw ""
    set key_rc [getch -noblock key_raw]
    set key ""
    if {$key_rc == 1} {
        set key [key_of $key_raw]
    } elseif {$key_rc == -1} {
        # stdin closed (e.g., piped run finished)
        set key "q"
    }

    set fired 0
    set moved_player 0

    if {[string length $key] > 0} {
        if {$key eq "q"} {
            set status "quit"
            set running 0
        } elseif {$key eq "f"} {
            if {[llength $bullets] < $max_bullets} {
                set spawn_x [expr {$player_x + $dir_x}]
                set spawn_y [expr {$player_y + $dir_y}]
                if {[walkable $spawn_x $spawn_y $width $height] == 1} {
                    lappend bullets [list $spawn_x $spawn_y $dir_x $dir_y $bullet_max_life]
                    set fired 1
                }
            }
        } elseif {$player_slow == 0} {
            if {$key eq "w"} {
                set moved [try_step $player_x $player_y 0 -1 $width $height]
                if {[lindex $moved 0] != $player_x || [lindex $moved 1] != $player_y} {
                    set moved_player 1
                }
                set player_x [lindex $moved 0]
                set player_y [lindex $moved 1]
                set dir_x 0
                set dir_y -1
            } elseif {$key eq "s"} {
                set moved [try_step $player_x $player_y 0 1 $width $height]
                if {[lindex $moved 0] != $player_x || [lindex $moved 1] != $player_y} {
                    set moved_player 1
                }
                set player_x [lindex $moved 0]
                set player_y [lindex $moved 1]
                set dir_x 0
                set dir_y 1
            } elseif {$key eq "a"} {
                set moved [try_step $player_x $player_y -1 0 $width $height]
                if {[lindex $moved 0] != $player_x || [lindex $moved 1] != $player_y} {
                    set moved_player 1
                }
                set player_x [lindex $moved 0]
                set player_y [lindex $moved 1]
                set dir_x -1
                set dir_y 0
            } elseif {$key eq "d"} {
                set moved [try_step $player_x $player_y 1 0 $width $height]
                if {[lindex $moved 0] != $player_x || [lindex $moved 1] != $player_y} {
                    set moved_player 1
                }
                set player_x [lindex $moved 0]
                set player_y [lindex $moved 1]
                set dir_x 1
                set dir_y 0
            }
        }
    }

    if {$moved_player == 1 && [base_tile $player_x $player_y $width $height] eq "~"} {
        # Swamp slows the next movement tick, but does not permanently root the player.
        set player_slow 1
    }

    set ptile_now [base_tile $player_x $player_y $width $height]
    if {$ptile_now eq "^"} {
        incr hp -1
    }

    set next_bullets {}
    foreach b $bullets {
        set bx [lindex $b 0]
        set by [lindex $b 1]
        set bdx [lindex $b 2]
        set bdy [lindex $b 3]
        set blife [expr {[lindex $b 4] - 1}]

        if {$blife < 0} {
            continue
        }

        set next_bx [expr {$bx + $bdx}]
        set next_by [expr {$by + $bdy}]
        if {[walkable $next_bx $next_by $width $height] == 0} {
            set _broken [break_wall $next_bx $next_by $width $height]
            continue
        }

        set hit 0
        if {$next_bx == $e1x && $next_by == $e1y} {
            incr score
            set pos [respawn_enemy [expr {$frame + 13}] $width $height]
            set e1x [lindex $pos 0]
            set e1y [lindex $pos 1]
            set hit 1
        }
        if {$hit == 0 && $next_bx == $e2x && $next_by == $e2y} {
            incr score
            set pos [respawn_enemy [expr {$frame + 29}] $width $height]
            set e2x [lindex $pos 0]
            set e2y [lindex $pos 1]
            set hit 1
        }

        if {$hit == 0} {
            lappend next_bullets [list $next_bx $next_by $bdx $bdy $blife]
        }
    }
    set bullets $next_bullets

    if {$frame % $enemy_every == 0} {
        thread channel send $cmd1 [list tick $e1x $e1y $player_x $player_y $frame]
        thread channel send $cmd2 [list tick $e2x $e2y $player_x $player_y $frame]

        set got1 0
        set got2 0
        while {$got1 == 0 || $got2 == 0} {
            set ev [thread channel recv $bus]
            set who [lindex $ev 1]
            set p_dx [lindex $ev 2]
            set p_dy [lindex $ev 3]
            set s_dx [lindex $ev 4]
            set s_dy [lindex $ev 5]

            if {$who eq "E1" && $got1 == 0} {
                if {$e1_slow > 0} {
                    incr e1_slow -1
                } else {
                    set moved [apply_enemy_move $e1x $e1y $p_dx $p_dy $s_dx $s_dy $player_x $player_y $width $height]
                    set e1x [lindex $moved 0]
                    set e1y [lindex $moved 1]
                    if {[base_tile $e1x $e1y $width $height] eq "~"} {
                        set e1_slow 1
                    }
                }
                set got1 1
            } elseif {$who eq "E2" && $got2 == 0} {
                if {$e2_slow > 0} {
                    incr e2_slow -1
                } else {
                    set moved [apply_enemy_move $e2x $e2y $p_dx $p_dy $s_dx $s_dy $player_x $player_y $width $height]
                    set e2x [lindex $moved 0]
                    set e2y [lindex $moved 1]
                    if {[base_tile $e2x $e2y $width $height] eq "~"} {
                        set e2_slow 1
                    }
                }
                set got2 1
            }
        }
    }

    if {$fired == 0 && [string length $key] > 0 && $key eq "f"} {
        incr score -1
        if {$score < 0} {
            set score 0
        }
    }

    if {$player_x == $gem_x && $player_y == $gem_y} {
        incr score 2
        set next [respawn_enemy [expr {$frame + 47}] $width $height]
        set gem_x [lindex $next 0]
        set gem_y [lindex $next 1]
    }

    if {($player_x == $e1x && $player_y == $e1y) || ($player_x == $e2x && $player_y == $e2y)} {
        incr hp -1
        set player_x 2
        set player_y [expr {$height - 3}]
    }

    if {$player_x == $goal_x && $player_y == $goal_y} {
        if {$score >= 5} {
            set status "victory"
            set running 0
        }
    }

    if {$hp <= 0} {
        set status "defeat"
        set running 0
    }

    if {$frame >= $max_frames} {
        set status "timeout"
        set running 0
    }

    set bullet_points {}
    foreach b $bullets {
        lappend bullet_points [list [lindex $b 0] [lindex $b 1]]
    }

    set frame_buf "$ansi_home"
    append frame_buf "=== FTCL Maze Raiders ===\n"
    append frame_buf [list frame $frame hp $hp score $score bullets [llength $bullets] goal_score 5 status $status tick_ms $tick_ms slow_cd $player_slow] "\n"
    append frame_buf [draw_board $width $height $player_x $player_y $e1x $e1y $e2x $e2y $bullet_points $gem_x $gem_y $goal_x $goal_y]
    append frame_buf "Controls: W/A/S/D move, F fire, Q quit\n"
    append frame_buf "Legend: # wall (inner walls breakable), ~ swamp, ^ trap, @ energy, G gate, 1/2 enemies, *=bullet, P player"
    puts $frame_buf

    set frame_sleep $tick_ms
    if {[string length $key] > 0} {
        set frame_sleep 0
    }
    sleep $frame_sleep
}

thread channel send $cmd1 [list stop]
thread channel send $cmd2 [list stop]
set w1 [thread await $ai1]
set w2 [thread await $ai2]

puts $ansi_show_cursor
puts [list workers $w1 $w2]
puts [list final_frame $frame final_hp $hp final_score $score final_status $status]
