idf_component_register(SRCS "sam.c" "render.c" "reciter.c" "debug.c"
                       INCLUDE_DIRS ".")
# 1982 code, reverse-engineered: keep its warnings out of the build log
target_compile_options(${COMPONENT_LIB} PRIVATE -w -fcommon) # -fcommon: sam.c and reciter.c both define A, X, Y
