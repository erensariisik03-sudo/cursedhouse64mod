        orig_TMP_SetCharacterValidation(instance, value, methodInfo);
    LogTMPInputState(instance, "set_characterValidation sonrasi");
}

static void my_TMP_SetInputValidator(void* instance, void* value, void* methodInfo) {
    LOGI("SET inputValidator field=%p validator=%p", instance, value);
    if (orig_TMP_SetInputValidator)
        orig_TMP_SetInputValidator(instance, value, methodInfo);
    LogTMPInputState(instance, "set_inputValidator sonrasi");
}

static void my_TMP_SetIsRichTextEditingAllowed(void* instance, int value, void* methodInfo) {
    LOGI("SET isRichTextEditingAllowed field=%p value=%d", instance, value);
    if (orig_TMP_SetIsRichTextEditingAllowed)
        orig_TMP_SetIsRichTextEditingAllowed(instance, value, methodInfo);
    LogTMPInputState(instance, "set_isRichTextEditingAllowed sonrasi");
}

static void my_TMP_SetRichText(void* instance, int value, void* methodInfo) {
    LOGI("SET richText field=%p value=%d", instance, value);
    if (orig_TMP_SetRichText)
        orig_TMP_SetRichText(instance, value, methodInfo);
    LogTMPInputState(instance, "set_richText sonrasi");
}

// ============================================================
// HOOK INSTALLATION
// ============================================================

static bool InstallHooks() {
    if (!g_il2cppLoadBias) {
        LOGE("libil2cpp load bias henuz hazir degil");
        return false;
    }

    LOGI("--- 32-BIT INPUT DIAGNOSTIC KANCALARI KURULUYOR ---");
    LOGI("TMP_InputField offsets: limit=0x114 validation=0x108 onValidate=0x138 validator=0x1F0 richText=0x16D richEdit=0x1E6 keyboard=0xB4 text=0x160");

    int successCount = 0;
    int attemptedCount = 0;

#define TRY_HOOK(rva, replacement, original, label) \
    do { \
        ++attemptedCount; \
        if (InstallArmHook((rva), reinterpret_cast<void*>(replacement), \
                reinterpret_cast<void**>(&(original)), (label))) ++successCount; \
    } while (0)

    // Existing limit hooks (kept)
    TRY_HOOK(kTMP_ActivateInputFieldInternal_RVA,
             my_TMP_ActivateInputFieldInternal,
             orig_TMP_ActivateInputFieldInternal,
             "TMP_InputField::ActivateInputFieldInternal");
    TRY_HOOK(kTMP_AppendChar_RVA, my_TMP_AppendChar,
             orig_TMP_AppendChar, "TMP_InputField::Append(char)");
    TRY_HOOK(kTMP_InsertChar_RVA, my_TMP_InsertChar,
             orig_TMP_InsertChar, "TMP_InputField::Insert(char)");
    TRY_HOOK(kTMP_AppendString_RVA, my_TMP_AppendString,
             orig_TMP_AppendString, "TMP_InputField::Append(string)");
    TRY_HOOK(kTMP_SetCharacterLimit_RVA, my_TMP_SetCharacterLimit,
             orig_TMP_SetCharacterLimit, "TMP_InputField::set_characterLimit");
    TRY_HOOK(kTSK_SetCharacterLimit_RVA, my_TSK_SetCharacterLimit,
             orig_TSK_SetCharacterLimit, "TouchScreenKeyboard::set_characterLimit");

    // New diagnostic hooks
    TRY_HOOK(kTMP_Validate_RVA, my_TMP_Validate,
             orig_TMP_Validate, "TMP_InputField::Validate");
    TRY_HOOK(kTMP_IsValidChar_RVA, my_TMP_IsValidChar,
             orig_TMP_IsValidChar, "TMP_InputField::IsValidChar");
    TRY_HOOK(kTMP_UpdateStringPositionFromKeyboard_RVA,
             my_TMP_UpdateStringPositionFromKeyboard,
             orig_TMP_UpdateStringPositionFromKeyboard,
             "TMP_InputField::UpdateStringPositionFromKeyboard");
    TRY_HOOK(kTMP_SetOnValidateInput_RVA, my_TMP_SetOnValidateInput,
             orig_TMP_SetOnValidateInput, "TMP_InputField::set_onValidateInput");
    TRY_HOOK(kTMP_SetCharacterValidation_RVA, my_TMP_SetCharacterValidation,
             orig_TMP_SetCharacterValidation, "TMP_InputField::set_characterValidation");
    TRY_HOOK(kTMP_SetInputValidator_RVA, my_TMP_SetInputValidator,
             orig_TMP_SetInputValidator, "TMP_InputField::set_inputValidator");
    TRY_HOOK(kTMP_SetIsRichTextEditingAllowed_RVA,
             my_TMP_SetIsRichTextEditingAllowed,
             orig_TMP_SetIsRichTextEditingAllowed,
             "TMP_InputField::set_isRichTextEditingAllowed");
    TRY_HOOK(kTMP_SetRichText_RVA, my_TMP_SetRichText,
             orig_TMP_SetRichText, "TMP_InputField::set_richText");

#undef TRY_HOOK

    LOGI("========== KANCA KURULUMU BITTI: %d/%d basarili ==========",
         successCount, attemptedCount);
    LOGI("Test: oyuncu adi, oda adi ve sohbet alanlarinda <size=999> girisini ayri ayri dene.");
    LOGI("Logcat filtre: CursedHouseChat");

    return successCount > 0;
}

// ============================================================
// THREAD / JNI
// ============================================================

static void* hack_thread(void*) {
    LOGI("CursedHouseChat diagnostic: hack_thread basladi; libil2cpp.so bekleniyor...");

    while (g_il2cppLoadBias == 0) {
        g_il2cppLoadBias = GetModuleLoadBias("libil2cpp.so");
        if (g_il2cppLoadBias == 0) sleep(1);
    }

    LOGI("libil2cpp load bias=0x%" PRIxPTR, g_il2cppLoadBias);
    InstallHooks();
    return nullptr;
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm;
    (void)reserved;

    pthread_t ptid;
    const int rc = pthread_create(&ptid, nullptr, hack_thread, nullptr);
    if (rc != 0) {
        LOGE("hack_thread olusturulamadi: %d", rc);
    } else {
        pthread_detach(ptid);
    }

    return JNI_VERSION_1_6;
}
