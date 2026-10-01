idf_component_register(SRCS "main.c" "llm.c"
                    INCLUDE_DIRS ""  LDFRAGMENTS "../linker.lf")



# https://github.com/espressif/esp-idf/issues/11696#issuecomment-1596208414
target_compile_options(${COMPONENT_LIB} PRIVATE -fno-if-conversion) # 


# The model and tokenizer are flashed as raw partitions and memory-mapped at
# runtime, so the weights are read straight from flash instead of RAM.
# 'idf.py -p PORT flash' writes them along with the app.
if(CONFIG_LLM_MODE_OFFICER)
    set(MODEL_FILE "${PROJECT_DIR}/data/officer.bin")
    set(TOKENIZER_FILE "${PROJECT_DIR}/data/officer_tok.bin")
else()
    set(MODEL_FILE "${PROJECT_DIR}/data/stories260K.bin")
    set(TOKENIZER_FILE "${PROJECT_DIR}/data/tok512.bin")
endif()
esptool_py_flash_to_partition(flash "model" "${MODEL_FILE}")
esptool_py_flash_to_partition(flash "tokenizer" "${TOKENIZER_FILE}")
