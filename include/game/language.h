#pragma once

#include "common.h"

// The game's two text files of each language: Language\Code\<language>.txt (the game's texts) and Language\AgentLab\<language>.txt
// (the scripts'), a text a line
enum TextFile : u32
{
    CodeTexts = 0,
    AgentLabTexts = 1,
    TextFiles = 2,
};

extern "C"
{
    // The languages: how many, their names (the folders and files of the Language files) and the current one
    extern u32 g_LanguageCount RETAIL(GlobalLanguagesAmount);
    extern const char** g_LanguageNames RETAIL(G_unkLanguagesStruct);
    extern u32 g_CurrentLanguage RETAIL(CurrentLanguageIndex);
    // The names the start-up gives (English, French, German, Spanish, Italian)
    extern const char* g_GameLanguageNames[] RETAIL(GlobalLanguagesArray);
    // Each text file's lines: how many line breaks the last one read had, every language's lines (none until read) and the
    // current language's
    extern u32 g_TextLineCounts[TextFiles] RETAIL(D_00309B00);
    extern const char*** g_LanguageTexts[TextFiles] RETAIL(D_00309B08);
    extern const char** g_Texts[TextFiles] RETAIL(D_00309B10);

    // The languages there are, every language's texts none yet
    void InitLanguages(u32 count, const char** names) RETAIL(InitAGlobalRelatedToLanguages);
    // The language played in: its texts the current ones (once there are languages), the game told
    void SetGameLanguage(u32 language) RETAIL(SetGameLanguage);
    // A text file's text (in memory that stays) split into its lines, the line breaks made ends; blank lines aren't lines, so
    // a text's number counts the lines that aren't blank. The lines are the language's and the current ones
    void ReadTextFile(u32 file, u32 language, char* text) RETAIL(FUN_0017e088);
    // The current language's texts the current ones, the game told
    void UseLanguageTexts() RETAIL(FUN_00181aa8);
}

// A game text by its number (the code's text file's)
inline const char* GameText(u32 text)
{
    return g_Texts[CodeTexts][text];
}
