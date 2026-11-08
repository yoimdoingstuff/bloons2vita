/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      fgsfds
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "utils/dialog.h"

#include <psp2/ctrl.h>
#include <psp2/ime_dialog.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/message_dialog.h>

#include <vitaGL.h>

#include "utils/glutil.h"

static uint16_t ime_title_utf16[SCE_IME_DIALOG_MAX_TITLE_LENGTH];
static uint16_t ime_initial_text_utf16[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
static uint16_t ime_input_text_utf16[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
static uint8_t ime_input_text_utf8[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];

void _utf16_to_utf8(const uint16_t *src, uint8_t *dst) {
    for (int i = 0; src[i]; i++) {
        if ((src[i] & 0xFF80) == 0) {
            *(dst++) = src[i] & 0xFF;
        } else if((src[i] & 0xF800) == 0) {
            *(dst++) = ((src[i] >> 6) & 0xFF) | 0xC0;
            *(dst++) = (src[i] & 0x3F) | 0x80;
        } else if((src[i] & 0xFC00) == 0xD800 && (src[i + 1] & 0xFC00) == 0xDC00) {
            *(dst++) = (((src[i] + 64) >> 8) & 0x3) | 0xF0;
            *(dst++) = (((src[i] >> 2) + 16) & 0x3F) | 0x80;
            *(dst++) = ((src[i] >> 4) & 0x30) | 0x80 | ((src[i + 1] << 2) & 0xF);
            *(dst++) = (src[i + 1] & 0x3F) | 0x80;
            i += 1;
        } else {
            *(dst++) = ((src[i] >> 12) & 0xF) | 0xE0;
            *(dst++) = ((src[i] >> 6) & 0x3F) | 0x80;
            *(dst++) = (src[i] & 0x3F) | 0x80;
        }
    }

    *dst = '\0';
}

void _utf8_to_utf16(const uint8_t *src, uint16_t *dst) {
    for (int i = 0; src[i];) {
        if ((src[i] & 0xE0) == 0xE0) {
            *(dst++) = ((src[i]&0x0F) <<12) | ((src[i+1]&0x3F) <<6) | (src[i+2]&0x3F);
            i += 3;
        } else if ((src[i] & 0xC0) == 0xC0) {
            *(dst++) = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
            i += 2;
        } else {
            *(dst++) = src[i];
            i += 1;
        }
    }

    *dst = '\0';
}

int init_ime_dialog(const char *title, const char *initial_text) {
    sceClibMemset(ime_title_utf16, 0, sizeof(ime_title_utf16));
    sceClibMemset(ime_initial_text_utf16, 0, sizeof(ime_initial_text_utf16));
    sceClibMemset(ime_input_text_utf16, 0, sizeof(ime_input_text_utf16));
    sceClibMemset(ime_input_text_utf8, 0, sizeof(ime_input_text_utf8));

    _utf8_to_utf16((uint8_t *)title, ime_title_utf16);
    _utf8_to_utf16((uint8_t *)initial_text, ime_initial_text_utf16);

    SceImeDialogParam param;
    sceImeDialogParamInit(&param);

    param.supportedLanguages = 0x0001FFFF;
    param.languagesForced = SCE_TRUE;
    param.type = SCE_IME_TYPE_BASIC_LATIN;
    param.title = ime_title_utf16;
    param.maxTextLength = SCE_IME_DIALOG_MAX_TEXT_LENGTH;
    param.initialText = ime_initial_text_utf16;
    param.inputTextBuffer = ime_input_text_utf16;

    return sceImeDialogInit(&param);
}

char *get_ime_dialog_result(void) {
    if (sceImeDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return NULL;

    SceImeDialogResult result;
    sceClibMemset(&result, 0, sizeof(SceImeDialogResult));
    sceImeDialogGetResult(&result);
    if (result.button == SCE_IME_DIALOG_BUTTON_ENTER)
        _utf16_to_utf8(ime_input_text_utf16, ime_input_text_utf8);
    sceImeDialogTerm();
    // For some reason analog stick stops working after ime
    sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);

    return (char *)ime_input_text_utf8;
}

int init_msg_dialog(const char *msg) {
    SceMsgDialogUserMessageParam msg_param;
    sceClibMemset(&msg_param, 0, sizeof(msg_param));
    msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
    msg_param.msg = (SceChar8 *)msg;

    SceMsgDialogParam param;
    sceMsgDialogParamInit(&param);
    _sceCommonDialogSetMagicNumber(&param.commonParam);
    param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
    param.userMsgParam = &msg_param;

    return sceMsgDialogInit(&param);
}

int get_msg_dialog_result(void) {
    if (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return 0;
    sceMsgDialogTerm();
    return 1;
}

static int msgbox_buttons = 0;
static SceMsgDialogButtonsParam msgbox_btn_param;
static char msgbox_text[512]; // SceMsgDialog user message size limit
static char msgbox_labels[3][64];

int msgbox_start(const char *title, const char *msg,
                 const char **buttons, int nbuttons) {
    if (nbuttons < 1) nbuttons = 1;
    if (nbuttons > 3) nbuttons = 3;
    msgbox_buttons = nbuttons;

    for (int i = 0; i < 3; i++) {
        const char *label = (i < nbuttons && buttons && buttons[i]) ? buttons[i] : "";
        sceClibSnprintf(msgbox_labels[i], sizeof(msgbox_labels[i]), "%s", label);
    }

    const char *t = (title && *title) ? title : NULL;
    const char *m = msg ? msg : "";

    if (nbuttons == 2) {
        // The system Yes/No dialog can't have custom labels, so spell out
        // which of the game's buttons each one stands for.
        sceClibSnprintf(msgbox_text, sizeof(msgbox_text),
                        "%s%s%s\n\n(Yes: %s / No: %s)",
                        t ? t : "", t ? "\n\n" : "", m,
                        msgbox_labels[0], msgbox_labels[1]);
    } else {
        sceClibSnprintf(msgbox_text, sizeof(msgbox_text), "%s%s%s",
                        t ? t : "", t ? "\n\n" : "", m);
    }

    SceMsgDialogUserMessageParam msg_param;
    sceClibMemset(&msg_param, 0, sizeof(msg_param));
    msg_param.msg = (SceChar8 *)msgbox_text;

    if (nbuttons == 1) {
        msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
    } else if (nbuttons == 2) {
        msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_YESNO;
    } else {
        sceClibMemset(&msgbox_btn_param, 0, sizeof(msgbox_btn_param));
        msgbox_btn_param.msg1 = (void *)msgbox_labels[0];
        msgbox_btn_param.msg2 = (void *)msgbox_labels[1];
        msgbox_btn_param.msg3 = (void *)msgbox_labels[2];
        msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS;
        msg_param.buttonParam = &msgbox_btn_param;
    }

    SceMsgDialogParam param;
    sceMsgDialogParamInit(&param);
    _sceCommonDialogSetMagicNumber(&param.commonParam);
    param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
    param.userMsgParam = &msg_param;

    return sceMsgDialogInit(&param);
}

int msgbox_poll(void) {
    if (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED)
        return -1;

    SceMsgDialogResult result;
    sceClibMemset(&result, 0, sizeof(result));
    sceMsgDialogGetResult(&result);
    sceMsgDialogTerm();

    // SceMsgDialogButtonId: 1 = OK/YES/BUTTON1, 2 = NO/BUTTON2, 3 = BUTTON3
    int idx;
    switch (result.buttonId) {
        case 1:
            idx = 0;
            break;
        case 2:
            idx = 1;
            break;
        case 3:
            idx = 2;
            break;
        default:
            // Dialog was cancelled: treat it as the last ("cancel") button.
            idx = msgbox_buttons - 1;
            break;
    }

    if (idx >= msgbox_buttons)
        idx = msgbox_buttons - 1;
    return idx;
}

void fatal_error(const char *fmt, ...) {
    va_list list;
    char string[512];

    va_start(list, fmt);
    sceClibVsnprintf(string, sizeof(string), fmt, list);
    va_end(list);

    if (!gl_initialized)
        vglInit(0);

    init_msg_dialog(string);

    while (!get_msg_dialog_result())
        vglSwapBuffers(GL_TRUE);

    sceKernelExitProcess(0);

    sceKernelExitProcess(0);
    while (1);
}

void warning(const char *msg) {
    SceMsgDialogUserMessageParam msg_param;
    sceClibMemset(&msg_param, 0, sizeof(SceMsgDialogUserMessageParam));
    msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
    msg_param.msg = (const SceChar8*)msg;
    SceMsgDialogParam param;
    sceMsgDialogParamInit(&param);
    param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
    param.userMsgParam = &msg_param;
    sceMsgDialogInit(&param);
    while (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
        vglSwapBuffers(GL_TRUE);
    }
    sceMsgDialogTerm();
}
