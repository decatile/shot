# shot

A CLI tool to kill processes with style

## What can it do for you?

shot allows you to send SIGKILL signal to specfic process ID and play gunshot sound (cooool)

## CLI usage

```bash
./shot -h                    -> print short summary

./shot [pids...]             -> [pids...] is gone!

./shot -f [non-existent-pid] -> ignore failures

./shot -q [non-existent-pid] -> no logs

./shot -s [sig] [pid]        -> send [sig] signal to [pid]

echo "<pid>" | ./shot        -> no pids == read from stdin
```

## Credits

- [miniaudio](https://github.com/mackron/miniaudio)
- [sound](https://pixabay.com/sound-effects/shot-and-reload-6158/)
