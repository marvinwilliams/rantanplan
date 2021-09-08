# Intro
This is rantanlan, a framework for solving classical planning problems given in pddl format.
The main idea of this planner is to only partially ground the planning problem before solving it.
The actual solving is done by translating the problem to SAT and using any
SAT-Solver supporting the ipasir interface.
Many options regarding how much to ground, what so actually solve and how to
encode the problem to SAT are provided.

# Instructions
## Build
To build, type

`cmake -B build -DCMAKE_BUILD_TYPE=Release`

`make -C build rantanplan_glucose`

# Usage
to use, type

`./build/rantanplan_glucose [Options...] <domain> <problem>`

available options can be obtained with

`./build/rantanplan_glucose --help`

The most interesting are
- `-t <n>` to specifiy the timeout in seconds
- `-m <mode>` to select the planning mode
  - fixed: Ground to target groundness and solve until timeout
  - oneshot: Ground incrementally and solve the smallest resulting encoding
  - interrupt: Ground incrementally and solve with each groundness until a given timeout is hit
  - parallel: Solve multiple encodings with different groundness at once
- `-r <n>` to specify the target groundness in `[0, 1]`
- `-e <encoding>` to specifiy the encoding
  - s: Sequential encoding
  - f: foreach encoding
  - lf: lifted foreach encoding
  - e: exists encoding
