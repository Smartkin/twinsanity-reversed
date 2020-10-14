packedBlobSize = int(input('Enter packed blob size:'), 16)
packedBlobSizeHelper = int(input('Enter packed blob size helper:'), 16)
boneSettings = (packedBlobSize & 0x7f)
transformations = (packedBlobSize >> 0xA & 0xFFE) // 2
timelines = (packedBlobSize >> 0x16)
blobSize = (packedBlobSize & 0x7F) * 0x8 + (packedBlobSize >> 0xA & 0xFFE) + (packedBlobSize >> 0x16) * packedBlobSizeHelper * 0x2
print('Blob size: ' + hex(blobSize))
print('Bone settings: ' + hex(boneSettings))
print('Transformations: ' + hex(transformations))
print('Timelines: ' + hex(timelines))
print('Bone settings length: ' + hex(boneSettings * 0x8))
print('Transformations length: ' + hex(transformations * 0x2))
print('Timelines length: ' + hex(timelines * packedBlobSizeHelper * 2))