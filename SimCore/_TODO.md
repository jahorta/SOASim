
1. work on reseting workers and resending jobs for stalled workers
1. try to remove frame limiter and test for consistency
1. method for storing completed battle tests
1. method for storing in-progress tests
1. start working on battle runner
    - one run until input start to collect battle info and provide to UI
    - jobs should provide:
        - save state
        - initial controller element positions
        - list of inputs for inputting battle
        - list of active predicates and breakpoints to test them
    - VM additions:
        - looping (goto? with set labels)
        - check predicates
        - return battle failure separate from other errors
    - Dolphin Wrapper additions:
        - read multiple at a time (iterate over std::vector of reads?) (for getting battle state) (can skip for first battle since it is predeterminined)
            - read at pointer? with vector of pointers that can be popped
            - set cur base address? with vector of base addresses that can popped
            - recursive reads? has extra overhead
    - 