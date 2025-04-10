Lucky & Aagam Penn Shell

# TODOs

There are a bunch of TODOs in the code. In addition, I've spotted some weird error conditions which i'll put here to investigate:

- [ ] piping in
    - `echo hello > test.txt`
    - `test.txt > cat`
    - see a "text file busy" exception which means that some part of that is an executable that is trying to be modified
    - maybe redirecting into cat does sonething weird?
- [ ] `cat test.txt` and `/bin/cat test.txt` both don't output anything (also `cat test.txt` should error, right?)
- [ ] really weird ctrl-d behavior

    hypothesis: command is hanging homehow but ctrl-c still outputs a print
    - need to add a handler that actually interrupts the child process
    ```
    penn-shell> /bin/echo | cat
    penn-shell> /bin/echo        

    penn-shell> /bin/echo "hello world" | wc -w
    ^C
    penn-shell> /bin/echo 
    ^C
    penn-shell> /bin/echo "hello world" | /bin/wc -w

    ^C
    penn-shell> ^C
    penn-shell> 
    ```

- [ ]

# Morning 2/27

Current state:
- can enqueue things onto the list using the `&`
- the commands execute in the BG

Remains to do:
1. ~~figure out why the waitpid(-1,. .., WNOHANG) (the while loop inside of the main loop) isn't detecting the dead background jobs~~
  1. was missing Wuntraced  
2. add function to dequeue a job by pid (what #1 should return to us)
  1. This requires us to track the PID of the process that wraps the job somewhere. We can probably do this in the job.pids array as it is currently unused, but we could also add a job.leader_pid
  2. Once we've done that, we can just write a function that finds the node with the right PID and extracts it 
3. complete handle_fg and handle_bg functions in jobs.c
