set(INCLUDE_DIRECTORIES
    #"../dmx4esp"
    "./unity"
    "." )

idf_component_register(SRCS "unit.c"
                       INCLUDE_DIRS ${INCLUDE_DIRECTORIES}
                       REQUIRES unity dmx4esp)