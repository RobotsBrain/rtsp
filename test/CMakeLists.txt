set(EXECUTABLE_OUTPUT_PATH ${OUTDIR}/test)

include_directories(
    ${TOPDIR}/include/
    ${TOPDIR}/src
)

link_directories(
)


#################################
add_executable(test_parse_rtp test_parse_rtp.c)
target_link_libraries(test_parse_rtp rtsp)


#################################
add_executable(test_parse_rtsp test_parse_rtsp.c)
target_link_libraries(test_parse_rtsp rtsp)


#################################
add_executable(test_parse_sdp test_parse_sdp.c)
target_link_libraries(test_parse_sdp rtsp)


#################################
add_executable(test_rtsp test_rtsp.c)
target_link_libraries(test_rtsp rtsp)


#################################
#add_executable(rtp_server rtp_server.c)
#target_link_libraries(rtp_server rtsp)


#################################
add_executable(test_rtsp_server test_rtsp_server.c)
target_link_libraries(test_rtsp_server rtsp pthread)