# stb image decoder and encoder

- Upstream: https://github.com/nothings/stb
- Commit: `d14d46f6b44456eb88beac58cba833a4135d4c89`
- Files: `stb_image.h`, `stb_image_write.h`
- License: the public-domain/MIT dual-license notices are included in each file.

Only JPEG and PNG decoding is enabled. Uploaded photos are decoded and
re-encoded before storage, so embedded metadata is not copied to the output.
