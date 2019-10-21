workspace "Process Scheduler"
  configurations {"Debug", "Release"}
  -- Use C as the target language for all builds
  language "C"
  -- cdialect "C11"
  -- location "build/projects"
  targetdir "build/bin/%{cfg.buildcfg}"


  configuration { "linux", "gmake" }

  filter "configurations:Debug"
    -- Add the preprocessor definition DEBUG to debug builds
    defines { "DEBUG" }
    -- Ensure symbols are bundled with debug builds
    symbols "On"
    buildoptions { "-Wall" }

  filter "configurations:Release"
    -- Add the preprocessor definition NDEBUG to release builds
    -- This should also disable assertions.
    defines { "NDEBUG" }
    -- Turn on compiler optimizations for release builds
    optimize "On"

  -- Scheduler build
  project "Scheduler"
    kind "ConsoleApp"
    -- recursively globs all .h and .c files from src folder.
    files { "src/**.h", "src/**.c" }
    removefiles { "src/interpreter.c",  "src/debugger.c"}
    links { "m" }
  
  -- Interpreter build
  project "Interpreter"
    kind "ConsoleApp"
    files { "src/shared_defs.h", "src/process.c", "src/interpreter.c" }
    dependson { "Scheduler" }
    
  project "ProcessTableTest"
    kind "ConsoleApp"
    defines { "TEST" }
    files { "src/**.h", "src/**.c" }
    files { "test/process_table_test.c" }
    files { "test/unity/*.h", "test/unity/*.c" }
    removefiles { "src/interpreter.c", "src/scheduler.c", "src/debugger.c" } 
    links { "m" }