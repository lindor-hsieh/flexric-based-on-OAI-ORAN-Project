add_executable(xapp_mac_ctrl
  mac_ctrl.c
  ../../../../src/util/alg_ds/alg/defer.c
  )

 target_link_libraries(xapp_mac_ctrl
 						PUBLIC
 						e42_xapp
 						-pthread
 						-lsctp
 						-ldl
 						)

# Node 3 Local xApp：整合 ZeroMQ 推論 + E2SM-MAC 閉環控制
add_executable(xapp_node3
  xapp_node3.c
  ../../../../src/util/alg_ds/alg/defer.c
  )

target_link_libraries(xapp_node3
                      PUBLIC
                      e42_xapp
                      -pthread
                      -lsctp
                      -ldl
                      -lzmq
                      -lcjson
                      )

# Node 4 Local xApp：整合 ZeroMQ 推論 + E2SM-MAC 閉環控制
add_executable(xapp_node4
  xapp_node4.c
  ../../../../src/util/alg_ds/alg/defer.c
  )

target_link_libraries(xapp_node4
                      PUBLIC
                      e42_xapp
                      -pthread
                      -lsctp
                      -ldl
                      -lzmq
                      -lcjson
                      )

# Node 1 Local xApp：整合 ZeroMQ 推論 + E2SM-MAC 閉環控制
add_executable(xapp_node1
  xapp_node1.c
  ../../../../src/util/alg_ds/alg/defer.c
  )

target_link_libraries(xapp_node1
                      PUBLIC
                      e42_xapp
                      -pthread
                      -lsctp
                      -ldl
                      -lzmq
                      -lcjson
                      )

# Node 2 Local xApp：整合 ZeroMQ 推論 + E2SM-MAC 閉環控制
add_executable(xapp_node2
  xapp_node2.c
  ../../../../src/util/alg_ds/alg/defer.c
  )

target_link_libraries(xapp_node2
                      PUBLIC
                      e42_xapp
                      -pthread
                      -lsctp
                      -ldl
                      -lzmq
                      -lcjson
                      )

# Node 5 Local xApp：整合 ZeroMQ 推論 + E2SM-MAC 閉環控制
add_executable(xapp_node5
  xapp_node5.c
  ../../../../src/util/alg_ds/alg/defer.c
  )

target_link_libraries(xapp_node5
                      PUBLIC
                      e42_xapp
                      -pthread
                      -lsctp
                      -ldl
                      -lzmq
                      -lcjson
                      )
