# Unified PRB Allocation xApp — 論文 fixed50/30/10/adaptive 策略
# PRB_POLICY 環境變數切換，無需重新編譯
add_executable(xapp_prb_alloc
  xapp_prb_alloc.c
  ../../../../src/util/alg_ds/alg/defer.c
  )

target_link_libraries(xapp_prb_alloc
                      PUBLIC
                      e42_xapp
                      -pthread
                      -lsctp
                      -ldl
                      -lm
                      )
