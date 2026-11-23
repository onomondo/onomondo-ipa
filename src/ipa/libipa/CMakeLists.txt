# Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

add_library(libipa STATIC
  activation_code.c
  bpp_segments.c
  es10a_get_euicc_cfg_addr.c
  es10b_add_init_eim.c
  es10b_auth_serv.c
  es10b_cancel_session.c
  es10b_euicc_mem_rst.c
  es10b_get_certs.c
  es10b_get_eim_cfg_data.c
  es10b_get_euicc_chlg.c
  es10b_get_euicc_info.c
  es10b_get_rat.c
  es10b_load_bnd_prfle_pkg.c
  es10b_load_euicc_pkg.c
  es10b_prep_dwnld.c
  es10b_prfle_rollback.c
  es10b_retr_notif_from_lst.c
  es10b_rm_notif_from_lst.c
  es10c_delete_prfle.c
  es10c_disable_prfle.c
  es10c_enable_prfle.c
  es10b_enable_using_dd.c
  es10c_get_eid.c
  es10c_get_prfle_info.c
  es10x.c
  esipa_auth_clnt.c
  esipa.c
  esipa_cancel_session.c
  esipa_get_bnd_prfle_pkg.c
  esipa_get_eim_pkg.c
  esipa_handle_notif.c
  esipa_init_auth.c
  esipa_prvde_eim_pkg_rslt.c
  euicc.c
  ipad.c
  log.c
  proc_cmn_cancel_sess.c
  proc_cmn_mtl_auth.c
  proc_indirect_prfle_dwnld.c
  proc_eim_pkg_retr.c
  proc_euicc_data_req.c
  proc_euicc_pkg_dwnld_exec.c
  proc_notif_delivery.c
  proc_prfle_dwnld.c
  proc_prfle_inst.c
  utils.c
)

target_include_directories(libipa PUBLIC ${CMAKE_SOURCE_DIR}/include PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libipa PUBLIC libasn)
target_compile_options(libipa PRIVATE -Wall)
if (M32)
  set_target_properties(libipa PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()
