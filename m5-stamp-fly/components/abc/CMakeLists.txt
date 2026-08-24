idf_component_register(
    SRCS "src/abc_app.c"
         "src/abc_cli.c"
         "src/abc_cmd.c"
         "src/abc_con.c"
         "src/abc_core.c"
         "src/abc_gen.c"
         "src/abc_mod.c"
         "src/abc_reg.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_timer esp_system
)
