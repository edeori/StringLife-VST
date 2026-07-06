/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   background_panel_png;
    const int            background_panel_pngSize = 2062105;

    extern const char*   button_on_png;
    const int            button_on_pngSize = 787304;

    extern const char*   button_off_png;
    const int            button_off_pngSize = 607184;

    extern const char*   guitarbass_on_png;
    const int            guitarbass_on_pngSize = 938712;

    extern const char*   guitarbass_off_png;
    const int            guitarbass_off_pngSize = 834921;

    extern const char*   knob_face_png;
    const int            knob_face_pngSize = 742578;

    extern const char*   slider_thumb_png;
    const int            slider_thumb_pngSize = 453766;

    extern const char*   moth_logo_png;
    const int            moth_logo_pngSize = 6187;

    extern const char*   OxaniumVariable_ttf;
    const int            OxaniumVariable_ttfSize = 43536;

    extern const char*   BarlowCondensedLight_ttf;
    const int            BarlowCondensedLight_ttfSize = 101996;

    extern const char*   BarlowCondensedMedium_ttf;
    const int            BarlowCondensedMedium_ttfSize = 103360;

    extern const char*   RajdhaniMedium_ttf;
    const int            RajdhaniMedium_ttfSize = 384348;

    extern const char*   licensing_config_json;
    const int            licensing_config_jsonSize = 498;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 13;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
