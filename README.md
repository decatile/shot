# shot

A CLI tool to kill processes with style

## What can it do for you?

shot allows you to send SIGKILL signal to specfic process ID and play gunshot sound (cooool)

## CLI usage

```bash
./shot [pid]                 -> pid is gone!

./shot [non-existent-pid]    -> failure

./shot -f [non-existent-pid] -> failure does not interrupt sound and other pids to kill

./shot -q [pid]              -> silence

./shot -h                    -> print short summary
```

## Credits

- [miniaudio](https://github.com/mackron/miniaudio)
- [sound](https://pixabay.com/sound-effects/shot-and-reload-6158/)
