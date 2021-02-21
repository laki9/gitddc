#include "widget.h"

#include <QApplication>
#include <ddcutil_types.h>
#include <ddcutil_c_api.h>
#include <ddcutil_macros.h>
#include <ddcutil_status_codes.h>

DDCA_Display_Ref     _dref;
DDCA_Display_Info*   _dinfo;

DDCA_Display_Handle  _dh = NULL;


void show_any_value(
        DDCA_Display_Handle     dh,
        DDCA_Vcp_Value_Type     value_type,
        DDCA_Vcp_Feature_Code   feature_code)
{
    DDCA_Status ddcrc;
    DDCA_Any_Vcp_Value * valrec;

    ddcrc = ddca_get_any_vcp_value_using_explicit_type(
            dh,
            feature_code,
            value_type,
            &valrec);
    if (ddcrc != 0) {
//        DDC_ERRMSG("ddca_get_any_vcp_value_using_explicit_type", ddcrc);
        goto bye;
    }

    if (valrec->value_type == DDCA_NON_TABLE_VCP_VALUE) {
       printf("Non-Table value: mh=0x%02x, ml=0x%02x, sh=0x%02x, ml=0x%02x\n",
              valrec->val.c_nc.mh,
              valrec->val.c_nc.ml,
              valrec->val.c_nc.sh,
              valrec->val.c_nc.sl);
       printf("As continuous value (if applicable): max value = %d, cur value = %d\n",
             valrec->val.c_nc.mh << 8 | valrec->val.c_nc.ml,    // or use macro VALREC_MAX_VAL()
             valrec->val.c_nc.sh << 8 | valrec->val.c_nc.sl);   // or use macro VALREC_CUR_VAL()
    }
    else {
       assert(valrec->value_type == DDCA_TABLE_VCP_VALUE);
       printf("Table value: 0x");
       for (int ndx=0; ndx<valrec->val.t.bytect; ndx++)
          printf("%02x", valrec->val.t.bytes[ndx]);
       puts("");
    }

 bye:
    return;
}

DDCA_Status perform_set_non_table_vcp_value(
      DDCA_Display_Handle    dh,
      DDCA_Vcp_Feature_Code  feature_code,
      uint8_t                hi_byte,
      uint8_t                lo_byte)
{
    bool saved_enable_verify = ddca_enable_verify(true);

    DDCA_Status ddcrc = ddca_set_non_table_vcp_value(dh, feature_code, hi_byte, lo_byte);
    if (ddcrc == DDCRC_VERIFY) {
        printf("Value verification failed.  Current value is now:\n");
        show_any_value(dh, DDCA_NON_TABLE_VCP_VALUE, feature_code);
     }
     else if (ddcrc != 0) {
//        DDC_ERRMSG("ddca_set_non_table_vcp_value", ddcrc);
     }
     else {
        printf("Setting new value succeeded.\n");
     }

     ddca_enable_verify(saved_enable_verify);
     return ddcrc;
}

bool
test_continuous_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code)
{
   DDCA_Status ddcrc;
   bool ok = false;
   const char * feature_name = ddca_get_feature_name(feature_code);
   printf("\nTesting get and set continuous value. dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, feature_name);


   printf("Resetting statistics...\n");
   ddca_reset_stats();

   bool create_default_if_not_found = false;
   DDCA_Feature_Metadata* info;
   ddcrc = ddca_get_feature_metadata_by_dh(
           feature_code,
           dh,
           create_default_if_not_found,
           &info);
   if (ddcrc != 0) {
       return ok;//goto bye;
   }
   if ( !(info->feature_flags & DDCA_CONT) ) {
      printf("Feature 0x%02x is not Continuous\n", feature_code);
      return ok;//goto bye;
   }

   DDCA_Non_Table_Vcp_Value valrec;
   ddcrc = ddca_get_non_table_vcp_value(
            dh,
            feature_code,
            &valrec);
   if (ddcrc != 0) {
//      DDC_ERRMSG("ddca_get_non_table_vcp_value", ddcrc);
      ok = false;
      return ok;//goto bye;
   }
   uint16_t max_val = valrec.mh << 8 | valrec.ml;
   uint16_t cur_val = valrec.sh << 8 | valrec.sl;

   printf("Feature 0x%02x (%s) current value = %d, max value = %d\n",
          feature_code, feature_name, cur_val, max_val);

   uint16_t old_value = cur_val;
   uint16_t new_value = old_value/2;
   printf("Setting new value %d,,,\n", new_value);
   uint8_t new_sh = new_value >> 8;
   uint8_t new_sl = new_value & 0xff;
   DDCA_Status ddcrc1 = perform_set_non_table_vcp_value(dh, feature_code, new_sh, new_sl);
   if (ddcrc1 != 0 && ddcrc1 != DDCRC_VERIFY)
       return ok;//goto bye;

bye:
    return ok;
}

bool
show_simple_nc_feature_value_by_table(
      DDCA_Feature_Value_Entry * feature_table,
      uint8_t                    feature_value)
{
    char * feature_value_name = NULL;
    bool ok = false;

    printf("Performing value lookup using ddca_get_simple_nc_feature_value_name_by_table\n");
    DDCA_Status rc =
    ddca_get_simple_nc_feature_value_name_by_table(
          feature_table,
          feature_value,
          &feature_value_name);
    if (rc != 0) {
//       DDC_ERRMSG("ddca_get_nc_feature_value_name_by_table", rc);
       printf("Unable to get interpretation of value 0x%02x\n",  feature_value);
       printf("Current value: 0x%02x\n", feature_value);
       ok = false;
    }
    else {
       printf("Current value: 0x%02x - %s\n", feature_value, feature_value_name);
       ok = true;
    }

    return ok;
}


bool
test_simple_nc_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code,
      uint8_t                 new_value)
{
    printf("\nTesting get and set of simple NC value: dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, ddca_get_feature_name(feature_code));

    printf("Resetting statistics...\n");
    ddca_reset_stats();
    DDCA_Status ddcrc;
    bool ok = false;

    DDCA_Feature_Metadata* info;
    ddcrc = ddca_get_feature_metadata_by_dh(
            feature_code,
            dh,
            false,              // create_default_if_not_found
            &info);
    if (ddcrc != 0) {
//       DDC_ERRMSG("ddca_get_feature_metadata_by_display", ddcrc);
        return ok;//goto bye;
    }

    if ( !(info->feature_flags & DDCA_SIMPLE_NC) ) {
       printf("Feature 0x%02x is not simple NC\n", feature_code);
       return ok;//goto bye;
    }

    DDCA_Non_Table_Vcp_Value valrec;
    ddcrc = ddca_get_non_table_vcp_value(
               dh,
               feature_code,
               &valrec);
    if (ddcrc != 0) {
        return ok;//goto bye;
    }
    printf("Feature 0x%02x current value = 0x%02x\n",
              feature_code,
              valrec.sl);
    uint8_t old_value = valrec.sl;

    ok = show_simple_nc_feature_value_by_table(info->sl_values, old_value);
    if (!ok)
        return ok;//goto bye;

    printf("Setting new value 0x%02x...\n", new_value);
    DDCA_Status ddcrc1 = perform_set_non_table_vcp_value(dh, feature_code, 0, new_value);
    if (ddcrc1 != 0 && ddcrc1 != DDCRC_VERIFY)
        return ok;//goto bye;

    printf("Resetting original value 0x%02x...\n", old_value);
    DDCA_Status ddcrc2 = perform_set_non_table_vcp_value(dh, feature_code, 0, old_value);
    if (ddcrc2 != 0 && ddcrc2 != DDCRC_VERIFY)
        return ok;//goto bye;

    if (ddcrc1 == 0 && ddcrc2 == 0)
       ok = true;

bye:

    return ok;
}

bool
test_complex_nc_value(
      DDCA_Display_Handle     dh,
      DDCA_Vcp_Feature_Code   feature_code)
{
   printf("\nTesting query of complex NC value: dh=%s, feature_code=0x%02x - %s\n",
           ddca_dh_repr(dh), feature_code, ddca_get_feature_name(feature_code));

    printf("Resetting statistics...\n");
    ddca_reset_stats();

    DDCA_Status ddcrc;
    bool ok = false;

    DDCA_Feature_Metadata* info;
    ddcrc = ddca_get_feature_metadata_by_dh(
           feature_code,
            dh,              // feature info can be MCCS version dependent
            false,           // create_default_if_not_found
            &info);
    if (ddcrc != 0) {
       goto bye;
    }
    assert(info->feature_flags & (DDCA_COMPLEX_NC|DDCA_NC_CONT));

    DDCA_Non_Table_Vcp_Value valrec;
    ddcrc = ddca_get_non_table_vcp_value(
            dh,
            feature_code,
            &valrec);
    if (ddcrc != 0) {
       goto bye;
    }

    printf("Feature 0x%02x current value: mh=0x%02x, ml=0x%02x, sh=0x%02x, sl=0x%02x\n",
              feature_code,
              valrec.mh,
              valrec.ml,
              valrec.sh,
              valrec.sl);

    char * formatted_value;
#ifdef ALT
    ddcrc = ddca_format_non_table_vcp_value(
                feature_code,
                info.vspec,
                info.mmid,
                &valrec,
                &formatted_value);
#endif
    ddcrc = ddca_format_non_table_vcp_value_by_dref(
                feature_code,
                ddca_display_ref_from_handle(dh),
                &valrec,
                &formatted_value);
    if (ddcrc != 0) {
       goto bye;
    }
    printf("Formatted value: %s\n", formatted_value);
    free(formatted_value);

    ok = true;
bye:
    return ok;
}




DDCA_Status perform_open_display(DDCA_Display_Handle * dh_loc) {

   DDCA_Status ddcrc = ddca_open_display2(_dref, false, dh_loc);
   if (ddcrc != 0) {
   }

   return ddcrc;
}

void setvcp(uint8_t feature_code, bool writeOnly, uint16_t shsl)
{
    bool debugFunc = false;
//    debugFunc = debugFunc || debugThread;
//    TRACECF(debugFunc, "Starting. feature_code=0x%02x.  shsl=0x%04x, writeOnly=%s",
//                       feature_code, shsl, SBOOL(writeOnly));

    uint8_t sh = (shsl >> 8);
    uint8_t sl = (shsl & 0xff);
//    TRACECF(debugFunc, "sh: 0x%02x, sl: 0x%02x", sh, sl);
    // rpt_ddca_status(feature_code, __func__, "ddca_bogus", 0);

    DDCA_Display_Handle dh;
    DDCA_Status ddcrc = perform_open_display(&dh);
    if (ddcrc == 0) {
       ddca_enable_verify(false);

       ddcrc = ddca_set_non_table_vcp_value(dh, feature_code, sh, sl);
       if (ddcrc != 0) {
//          TRACECF(debugFunc, "ddca_set_non_table_vcp_value() returned %d - %s", ddcrc, ddca_rc_name(ddcrc));
//          rpt_ddca_status(feature_code, __func__, "ddca_set_non_table_vcp_value", ddcrc);
          goto bye;
       }
       if (!writeOnly) {
           DDCA_Non_Table_Vcp_Value  valrec;
           ddcrc = ddca_get_non_table_vcp_value(dh, feature_code, &valrec);
           if (ddcrc != 0) {
//                rpt_ddca_status(feature_code, __func__, "ddca_get_nontable_vcp_value", ddcrc);
           }
           else {

              if ((sl != valrec.sl || sh != valrec.sh)) {
                 // TRACE("Calling rpt_verify_error()");
//                 rpt_verify_error(feature_code, "ddca_set_non_table_vcp_value", sh, sl, valrec.sh, valrec.sl);
              }    // ddca_set_non_table_vcp_value() succeeded
              // 10/2019 coming here even if verify error???
//              TRACECF(debugFunc, "Calling _baseModel->modelVcpValueUpdate()");
           }  // ddca_get_non_table_vcp_value() succeeded
       }     // !writeOnly

bye:
       ddcrc = ddca_close_display(dh);
       if (ddcrc != 0) {
       }
    }   // open succeeded
}


int main(int argc, char *argv[])
{
    int which_test = 0;
    DDCA_Status rc;
    DDCA_Display_Ref dref;
    DDCA_Display_Handle dh = NULL;  // initialize to avoid clang analyzer warning
    int MAX_DISPLAYS = 4;           // limit the number of displays

    DDCA_Display_Info_List* dlist = NULL;
    ddca_get_display_info_list2(
          false,
          &dlist);
    for (int ndx = 0; ndx <  dlist->ct && ndx < MAX_DISPLAYS; ndx++) {
       DDCA_Display_Info * dinfo = &dlist->info[ndx];
       // For all the gory details:
       ddca_report_display_info(dinfo, /* depth=*/ 1);
       dref = dinfo->dref;

       rc = ddca_open_display2(dref, false, &dh);
       if (rc != 0) {
          continue;
       }

       if (which_test == 0 || which_test == 1)
          test_continuous_value(dh, 0x10);

       if (which_test == 0 || which_test == 3)
          test_complex_nc_value(dh, 0xDF);    // VCP version

       rc = ddca_close_display(dh);
       if (rc != 0)
          printf("ddca_close_display error\n");
       dh = NULL;
    }

    ddca_free_display_info_list(dlist);
}
