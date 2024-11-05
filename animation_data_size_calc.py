packedAnimationDataSize = int(input('Enter packed animation data size:'), 16)
packedBlobSizeHelper = int(input('Enter packed animation data size helper:'), 16)

jointSettings = (packedAnimationDataSize & 0x7f)
staticTransforms = (packedAnimationDataSize >> 0xA & 0xFFE) // 2
animatedTransforms = (packedAnimationDataSize >> 0x16)
animationDataSize = (packedAnimationDataSize & 0x7F) * 0x8 + (packedAnimationDataSize >> 0xA & 0xFFE) + (packedAnimationDataSize >> 0x16) * packedBlobSizeHelper * 0x2

print('Animation data size: ' + hex(animationDataSize))
print('Joint settings: ' + hex(jointSettings))
print('Static transforms: ' + hex(staticTransforms))
print('Animated transforms: ' + hex(animatedTransforms))
print('Joint settings length: ' + hex(jointSettings * 0x8))
print('Static transforms length: ' + hex(staticTransforms * 0x2))
print('Animated transforms length: ' + hex(animatedTransforms * packedBlobSizeHelper * 2))