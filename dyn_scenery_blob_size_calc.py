packedBlobSize = int(input('Enter packed blob size:'), 16)
packedBlobSizeHelper = int(input('Enter packed blob size helper:'), 16)
unk = (packedBlobSize & 0x7f)
unk2 = (packedBlobSize >> 0x9 & 0x1FFC) // 4
unk3 = (packedBlobSize >> 0x16)
blobSize = (packedBlobSize & 0x7F) * 0x8 + (packedBlobSize >> 0x9 & 0x1FFC) + (packedBlobSize >> 0x16) * packedBlobSizeHelper * 0x4
print('Blob size: ' + hex(blobSize))
print('Unknowns: ' + hex(unk))
print('Unknowns 2: ' + hex(unk2))
print('Unknowns 3: ' + hex(unk3))
print('Unk length: ' + hex(unk * 0x8))
print('Unk2 length: ' + hex(unk2 * 0x4))
print('Unk3 length: ' + hex(unk3 * packedBlobSizeHelper * 0x4))