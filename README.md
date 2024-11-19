# _Sample project_

This project created for learning FreeRTOS features. For results, ESP must be run with **IDF version 5.3**. This code has a dependency on **ESP IDF version 5.3**.


Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  This is the file you are currently reading
```
Additionally, the sample project contains Makefile and component.mk files, used for the legacy Make based build system. 
They are not used or needed when building with CMake and idf.py.
