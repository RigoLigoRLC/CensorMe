#ifndef DEFS_H
#define DEFS_H

enum CensorType {
    CT_Pixelize,
    CT_GaussianBlur,
    CT_White
};

enum PreviewMode {
    PM_Original,
    PM_FullyCensored,
    PM_MaskOnly,
    PM_MaskOnImage,
    PM_FinalPreview,
};

constexpr const char* CensorMeDataDir = "CensorMeData";

#endif // DEFS_H
