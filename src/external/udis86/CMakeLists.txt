file(GLOB SOURCE_UDIS86
  *.h
  libudis86/*.c
  libudis86/*.h)

add_library(lib_udis86 ${SOURCE_UDIS86})

