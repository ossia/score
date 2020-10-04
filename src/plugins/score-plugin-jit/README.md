# score-addon-jit

An addon that uses LLVM to JIT-compile score addons.

Needs :

- LLVM 7

# TODO

- When the code of an addon is modified, deserialize and reserialize the relevant data.
-> we need to tag the object pointers somemwho, and track where they have been going ?
-> interfaces must track the objects they allocate and have standard ways to hotswap / reload / remove them

- When addons are downloaded, the QFileSystemWatcher triggers even though the extraction hasn't finisehd -> build fail
-> disable them while extracting and scan the new folder ? wait 5 seconds when a new folder is created ?

- setup interfaces aren't registered
