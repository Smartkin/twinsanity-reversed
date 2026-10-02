#include "game/language.h"

#include "game/context.h"
#include "game/memory.h"

void InitLanguages(u32 count, const char** names)
{
    g_LanguageNames = names;
    g_LanguageCount = count;
    for (u32 file = 0; file < TextFiles; file++)
    {
        g_LanguageTexts[file] = static_cast<const char***>(MemoryAllocate2(count * sizeof(const char**)));
        for (u32 language = 0; language < count; language++)
        {
            g_LanguageTexts[file][language] = nullptr;
        }
    }
}

void ReadTextFile(u32 file, u32 language, char* text)
{
    u32 breaks = 0;
    char* at = text;
    while (*at != '\0')
    {
        if ((at[0] == '\r' && at[1] == '\n') || (at[0] == '\n' && at[1] == '\r'))
        {
            at[0] = '\0';
            at[1] = '\0';
            breaks++;
            at += 2;
        }
        else if (at[0] == '\n' || at[0] == '\r')
        {
            at[0] = '\0';
            breaks++;
            at++;
        }
        else
        {
            at++;
        }
    }

    // A line for every line break, blank lines skipped: with blank lines the last ones are found past the text's end (retail)
    auto* lines = static_cast<const char**>(MemoryAllocate2(breaks * sizeof(const char*)));
    at = text;
    for (u32 line = 0; line < breaks; line++)
    {
        lines[line] = at;
        while (*at++ != '\0')
        {
        }

        while (*at == '\0')
        {
            at++;
        }
    }

    g_TextLineCounts[file] = breaks;
    g_Texts[file] = lines;
    g_LanguageTexts[file][language] = lines;
}

void UseLanguageTexts()
{
    for (u32 file = 0; file < TextFiles; file++)
    {
        g_Texts[file] = g_LanguageTexts[file][g_CurrentLanguage];
    }

    g_GameContext->LanguageChanged(g_CurrentLanguage);
}

void SetGameLanguage(u32 language)
{
    g_CurrentLanguage = language;
    if (g_LanguageCount != 0)
    {
        UseLanguageTexts();
    }
}
