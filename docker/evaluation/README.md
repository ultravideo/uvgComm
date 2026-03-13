# uvgComm Evaluation Scripts

This folder contains example scripts for automating tests and evaluations with **uvgComm**. The uvgComm scripting language allows you to control calls, adjust settings, and exit the application without manual interaction.

---

## Running a Script

When starting uvgComm, pass the path to a script file as an argument:

```bash
uvgComm --script /path/to/script.txt
````

Or if you prefer, you may feed commands through stdin with:

```bash
cat /path/to/script.txt | ./uvgComm --script stdin
````

The commands in the script will be executed sequentially. If no script is specified, uvgComm starts normally in interactrive mode.

---

## Script Commands

| Command    | Aliases          | Parameters             | Description                                                                                 |
| ---------- | ---------------- | ---------------------- | ------------------------------------------------------------------------------------------- |
| `call`     | -                | `<username> <address>` | Sends an INVITE to the specified username at the given address.                             |
| `wait`     | -                | `<time in seconds>`    | Waits set amount of time before executing the next order. Useful for pacing scripts.        |
| `hangup`   | `end`, `endcall` | -                      | Ends the current call.                                                                      |
| `setting`  | `settingsValue`  | `<name> <value>`       | Modifies a setting value in the program (does not apply until a set command is issued).     |
| `setVideo` | -                | -                      | Applies any pending changes to video settings.                                              |
| `setAudio` | -                | -                      | Applies any pending changes to audio settings.                                              |
| `setCall`  | -                | -                      | Applies any pending changes to call settings.                                               |
| `quit`     | `exit`, `close`  | -                      | Shuts down uvgComm.                                                                         |

---

## Best Practices

* **End with `quit`** to ensure the application stops after running the script.
* Use `wait` to simulate realistic call durations or pauses between commands.
* A typical flow for a script is 
* Use `setting` commands before `setVideo`, `setAudio`, or `setCall` to update configuration. List of current setting values is listed [here]{src/settingskeys.h}

---

## Example Scripts

### Example 1: Basic Call

```text
call alice 192.168.0.101
wait 10
hangup
quit
```

Calls `alice`, waits 10 seconds, hangs up, and exits.

### Example 2: Changing Video Settings Before Call

```text
call alice 192.168.0.101
wait 10
hangup
setting resolution 1280x720
setting framerate 30
setVideo
call bob 192.168.0.102
hangup
quit
```


