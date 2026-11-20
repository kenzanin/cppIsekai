# --os:standalone has no panic machinery; the guest never reports one.
proc rawoutput(s: string) {.noinline.} = discard
proc panic(s: string) {.noinline.} = discard
proc fatalWriteln(s: string) {.noinline.} = discard
