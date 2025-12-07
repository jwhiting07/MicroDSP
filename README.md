# GhostWire Audio: Road to DSP – MicroDSP Projects

A collection of tiny, beginner-friendly DSP projects created as part of my social media series on learning digital audio programming. Each project is intentionally small, focused on one concept, and written with **extensive comments** so beginners can understand the code step by step. Over time, the projects will get progressively more complex as the series goes on.

---

## What’s Inside

Each project has:

- A fully commented `project_name.cpp`
- Optional example audio or diagrams

Topics include:

- Oscillators (sine, saw, phase accumulator)
- Simple filters (one-pole LP/HP, moving average)
- Delay lines, buffers, micro-effects
- Creative tools like bitcrushers and waveshapers
- Utilities like WAV writers and simple samplers

New micro-projects will be added regularly as GhostWire Audio grows.

---

## How to Run a Project
Navigate to the desired directory
```bash
git clone https://github.com/jwhiting07/MicroDSP.git
cd MicroDSP
g++ file_name.cpp -o executable_name
./executable_name
```

A standard C++17 compiler is all you need for most projects.

## Why MicroDSP?
GhostWire Audio aims to make DSP approachable by breaking it down into small, digestible building blocks—perfect for learners, musicians, indie plugin developers, and curious programmers.

## License
MIT License — free to use, modify, and build upon.

### I will eventually write either a README for each project, or a full paper explaining the importance and use cases of each concept.
