# Android ndk makefile for desmume

LOCAL_PATH := $(call my-dir)
LOCAL_BUILD_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    		:= 	libdesmumev7
LOCAL_C_INCLUDES		:= 	$(LOCAL_PATH)/../../ \
							$(LOCAL_PATH)/desmume \
							$(LOCAL_PATH)/desmume/7z/CPP \
							$(LOCAL_PATH)/desmume/7z/CPP/include_windows \
							$(LOCAL_PATH)/desmume/7z/CPP/myWindows \
							$(LOCAL_PATH)/desmume/agg/include
						   
LOCAL_SRC_FILES			:= 	../../addons/slot1_none.cpp \
							../../addons/slot1_r4.cpp \
							../../addons/slot1_retail.cpp \
							../../addons/slot1_retail_nand.cpp \
							../../addons/slot2_expMemory.cpp \
							../../addons/slot2_gbagame.cpp \
							../../addons/slot2_guitarGrip.cpp \
							../../addons/slot2_mpcf.cpp \
							../../addons/slot2_none.cpp \
							../../addons/slot2_paddle.cpp \
							../../addons/slot2_piano.cpp \
							../../addons/slot2_rumblepak.cpp \
							../../utils/decrypt/crc.cpp \
							../../utils/decrypt/decrypt.cpp \
							../../utils/decrypt/header.cpp \
							../../utils/libfat/cache.cpp \
							../../utils/libfat/directory.cpp \
							../../utils/libfat/disc.cpp \
							../../utils/libfat/fatfile.cpp \
							../../utils/libfat/fatdir.cpp \
							../../utils/libfat/file_allocation_table.cpp \
							../../utils/libfat/filetime.cpp \
							../../utils/libfat/libfat.cpp \
							../../utils/libfat/libfat_public_api.cpp \
							../../utils/libfat/lock.cpp \
							../../utils/libfat/partition.cpp \
							../../utils/tinycc/libtcc.c \
							../../utils/tinyxml/tinyxml.cpp \
							../../utils/tinyxml/tinyxmlparser.cpp \
							../../utils/tinyxml/tinyxmlerror.cpp \
							../../utils/tinyxml/tinystr.cpp \
							../../utils/ConvertUTF.c \
							../../utils/datetime.cpp \
							../../utils/dlditool.cpp \
							../../utils/emufat.cpp \
							../../utils/FileMap.cpp \
							../../utils/guid.cpp \
							../../utils/md5.cpp \
							../../utils/MemBuffer.cpp \
							../../utils/task.cpp \
							../../utils/vfat.cpp \
							../../utils/xstring.cpp \
							../../metaspu/metaspu.cpp \
							../../filter/2xsai.cpp \
							../../filter/bilinear.cpp \
							../../filter/epx.cpp \
							../../filter/hq2x.cpp \
							../../filter/hq4x.cpp \
							../../filter/lq2x.cpp \
							../../filter/scanline.cpp \
							../../addons.cpp \
							../../aggdraw.cpp \
							../../arm_instructions.cpp \
							../../ArmAnalyze.cpp \
							../../armcpu.cpp \
							../../ArmCJit.cpp \
							../../ArmThreadedInterpreter.cpp \
							../../bios.cpp \
							../../cheatSystem.cpp \
							../../common.cpp \
							../../cp15.cpp \
							../../CpuBase.cpp \
							../../debug.cpp \
							../../Disassembler.cpp \
							../../driver.cpp \
							../../emufile.cpp \
							../../FIFO.cpp \
							../../firmware.cpp \
							../../fs-linux.cpp \
							../../gfx3d.cpp \
							../../GPU.cpp \
							../../GPU_osd.cpp \
							../../JitBase.cpp \
							../../matrix.cpp \
							../../mc.cpp \
							../../MMU.cpp \
							../../movie.cpp \
							../../NDSSystem.cpp \
							../../OGLRender.cpp \
							../../path.cpp \
							../../rasterize.cpp \
							../../readwrite.cpp \
							../../render3D.cpp \
							../../ROMReader.cpp \
							../../rtc.cpp \
							../../saves.cpp \
							../../slot1.cpp \
							../../SPU.cpp \
							../../texcache.cpp \
							../../thumb_instructions.cpp \
							../../version.cpp \
							../../wifi.cpp \
							desmume/7z/C/Threads.c \
							desmume/7z/C/LzmaDec.c \
							desmume/7z/C/LzmaEnc.c \
							desmume/7z/C/LzFind.c \
							desmume/7z/C/7zCrc.c \
							desmume/7z/C/Aes.c \
							desmume/7z/C/HuffEnc.c \
							desmume/7z/C/Sort.c \
							desmume/7z/C/BwtSort.c \
							desmume/7z/CPP/7zip/Archive/ArchiveExports.cpp \
							desmume/7z/CPP/7zip/Archive/DllExports2.cpp \
							desmume/7z/CPP/7zip/Archive/Common/ItemNameUtils.cpp \
							desmume/7z/CPP/7zip/Archive/Common/OutStreamWithCRC.cpp \
							desmume/7z/CPP/7zip/Archive/Common/InStreamWithCRC.cpp \
							desmume/7z/CPP/7zip/Archive/Common/ParseProperties.cpp \
							desmume/7z/CPP/7zip/Archive/Common/HandlerOut.cpp \
							desmume/7z/CPP/7zip/Archive/Common/CoderMixer2MT.cpp \
							desmume/7z/CPP/7zip/Archive/Common/CoderMixer2.cpp \
							desmume/7z/CPP/7zip/Archive/Common/DummyOutStream.cpp \
							desmume/7z/CPP/7zip/Archive/Common/FindSignature.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zCompressionMode.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zDecode.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zEncode.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zExtract.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zFolderInStream.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zFolderOutStream.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zHandler.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zHandlerOut.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zHeader.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zIn.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zOut.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zProperties.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zRegister.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zSpecStream.cpp \
							desmume/7z/CPP/7zip/Archive/7z/7zUpdate.cpp \
							desmume/7z/CPP/7zip/Archive/Rar/RarHandler.cpp \
							desmume/7z/CPP/7zip/Archive/Rar/RarHeader.cpp \
							desmume/7z/CPP/7zip/Archive/Rar/RarIn.cpp \
							desmume/7z/CPP/7zip/Archive/Rar/RarItem.cpp \
							desmume/7z/CPP/7zip/Archive/Rar/RarRegister.cpp \
							desmume/7z/CPP/7zip/Archive/Rar/RarVolumeInStream.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipAddCommon.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipHandler.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipHandlerOut.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipHeader.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipIn.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipItem.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipOut.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipRegister.cpp \
							desmume/7z/CPP/7zip/Archive/Zip/ZipUpdate.cpp \
							desmume/7z/CPP/7zip/Archive/BZip2/bz2Register.cpp \
							desmume/7z/CPP/7zip/Archive/BZip2/BZip2Handler.cpp \
							desmume/7z/CPP/7zip/Archive/BZip2/BZip2HandlerOut.cpp \
							desmume/7z/CPP/7zip/Archive/BZip2/BZip2Update.cpp \
							desmume/7z/CPP/Common/MyWindows.cpp \
							desmume/7z/CPP/Common/MyString.cpp \
							desmume/7z/CPP/Common/StringConvert.cpp \
							desmume/7z/CPP/Common/MyVector.cpp \
							desmume/7z/CPP/Common/UTFConvert.cpp \
							desmume/7z/CPP/Common/IntToString.cpp \
							desmume/7z/CPP/Common/StringToInt.cpp \
							desmume/7z/CPP/Windows/PropVariant.cpp \
							desmume/7z/CPP/Windows/PropVariantConversions.cpp \
							desmume/7z/CPP/Windows/System.cpp \
							desmume/7z/CPP/Windows/Time.cpp \
							desmume/7z/CPP/Windows/Synchronization.cpp \
							desmume/7z/CPP/Windows/FileIO.cpp \
							desmume/7z/CPP/Windows/FileFind.cpp \
							desmume/7z/CPP/Windows/FileDir.cpp \
							desmume/7z/CPP/myWindows/wine_date_and_time.cpp \
							desmume/7z/CPP/myWindows/myGetTickCount.cpp \
							desmume/7z/CPP/7zip/Common/CreateCoder.cpp \
							desmume/7z/CPP/7zip/Common/FilterCoder.cpp \
							desmume/7z/CPP/7zip/Common/StreamUtils.cpp \
							desmume/7z/CPP/7zip/Common/ProgressUtils.cpp \
							desmume/7z/CPP/7zip/Common/LimitedStreams.cpp \
							desmume/7z/CPP/7zip/Common/MemBlocks.cpp \
							desmume/7z/CPP/7zip/Common/OutMemStream.cpp \
							desmume/7z/CPP/7zip/Common/ProgressMt.cpp \
							desmume/7z/CPP/7zip/Common/OutBuffer.cpp \
							desmume/7z/CPP/7zip/Common/InBuffer.cpp \
							desmume/7z/CPP/7zip/Common/StreamObjects.cpp \
							desmume/7z/CPP/7zip/Common/LockedStream.cpp \
							desmume/7z/CPP/7zip/Common/VirtThread.cpp \
							desmume/7z/CPP/7zip/Common/InOutTempBuffer.cpp \
							desmume/7z/CPP/7zip/Common/MethodProps.cpp \
							desmume/7z/CPP/7zip/Common/OffsetStream.cpp \
							desmume/7z/CPP/7zip/Common/MethodId.cpp \
							desmume/7z/CPP/7zip/Common/StreamBinder.cpp \
							desmume/7z/CPP/7zip/Compress/CodecExports.cpp \
							desmume/7z/CPP/7zip/Compress/CopyCoder.cpp \
							desmume/7z/CPP/7zip/Compress/LzmaDecoder.cpp \
							desmume/7z/CPP/7zip/Compress/LzmaEncoder.cpp \
							desmume/7z/CPP/7zip/Compress/ImplodeDecoder.cpp \
							desmume/7z/CPP/7zip/Compress/ShrinkDecoder.cpp \
							desmume/7z/CPP/7zip/Compress/ImplodeHuffmanDecoder.cpp \
							desmume/7z/CPP/7zip/Compress/LzOutWindow.cpp \
							desmume/7z/CPP/7zip/Compress/BitlDecoder.cpp \
							desmume/7z/CPP/7zip/Compress/DeflateDecoder.cpp \
							desmume/7z/CPP/7zip/Compress/DeflateEncoder.cpp \
							desmume/7z/CPP/7zip/Compress/DeflateRegister.cpp \
							desmume/7z/CPP/7zip/Compress/LzmaRegister.cpp \
							desmume/7z/CPP/7zip/Compress/RarCodecsRegister.cpp \
							desmume/7z/CPP/7zip/Compress/Rar1Decoder.cpp \
							desmume/7z/CPP/7zip/Compress/Rar2Decoder.cpp \
							desmume/7z/CPP/7zip/Compress/Rar3Decoder.cpp \
							desmume/7z/CPP/7zip/Compress/Rar3Vm.cpp \
							desmume/7z/CPP/7zip/Compress/BZip2Crc.cpp \
							desmume/7z/CPP/7zip/Compress/BZip2Decoder.cpp \
							desmume/7z/CPP/7zip/Compress/BZip2Encoder.cpp \
							desmume/7z/CPP/7zip/Compress/BZip2Register.cpp \
							desmume/7z/CPP/7zip/Crypto/ZipStrong.cpp \
							desmume/7z/CPP/7zip/Crypto/ZipCrypto.cpp \
							desmume/7z/CPP/7zip/Crypto/Sha1.cpp \
							desmume/7z/CPP/7zip/Crypto/RandGen.cpp \
							desmume/7z/CPP/7zip/Crypto/MyAes.cpp \
							desmume/7z/CPP/7zip/Crypto/Pbkdf2HmacSha1.cpp \
							desmume/7z/CPP/7zip/Crypto/HmacSha1.cpp \
							desmume/7z/CPP/7zip/Crypto/RarAes.cpp \
							desmume/7z/CPP/7zip/Crypto/Rar20Crypto.cpp \
							desmume/7z/CPP/7zip/Crypto/WzAes.cpp \
							desmume/agg/src/agg_arc.cpp \
							desmume/agg/src/agg_arrowhead.cpp \
							desmume/agg/src/agg_bezier_arc.cpp \
							desmume/agg/src/agg_bspline.cpp \
							desmume/agg/src/agg_curves.cpp \
							desmume/agg/src/agg_embedded_raster_fonts.cpp \
							desmume/agg/src/agg_gsv_text.cpp \
							desmume/agg/src/agg_image_filters.cpp \
							desmume/agg/src/agg_line_aa_basics.cpp \
							desmume/agg/src/agg_line_profile_aa.cpp \
							desmume/agg/src/agg_rounded_rect.cpp \
							desmume/agg/src/agg_sqrt_tables.cpp \
							desmume/agg/src/agg_trans_affine.cpp \
							desmume/agg/src/agg_trans_double_path.cpp \
							desmume/agg/src/agg_trans_single_path.cpp \
							desmume/agg/src/agg_trans_warp_magnifier.cpp \
							desmume/agg/src/agg_vcgen_bspline.cpp \
							desmume/agg/src/agg_vcgen_contour.cpp \
							desmume/agg/src/agg_vcgen_dash.cpp \
							desmume/agg/src/agg_vcgen_markers_term.cpp \
							desmume/agg/src/agg_vcgen_smooth_poly1.cpp \
							desmume/agg/src/agg_vcgen_stroke.cpp \
							desmume/agg/src/agg_vpgen_clip_polygon.cpp \
							desmume/agg/src/agg_vpgen_clip_polyline.cpp \
							desmume/agg/src/agg_vpgen_segmentator.cpp \
							desmume/agg/src/ctrl/agg_bezier_ctrl.cpp \
							desmume/agg/src/ctrl/agg_cbox_ctrl.cpp \
							desmume/agg/src/ctrl/agg_gamma_ctrl.cpp \
							desmume/agg/src/ctrl/agg_gamma_spline.cpp \
							desmume/agg/src/ctrl/agg_polygon_ctrl.cpp \
							desmume/agg/src/ctrl/agg_rbox_ctrl.cpp \
							desmume/agg/src/ctrl/agg_scale_ctrl.cpp \
							desmume/agg/src/ctrl/agg_slider_ctrl.cpp \
							desmume/agg/src/ctrl/agg_spline_ctrl.cpp \
							desmume/mic.cpp \
							desmume/throttle.cpp \
							desmume/main.cpp \
							desmume/OpenArchive.cpp \
							desmume/7zip.cpp \
							desmume/sndopensl.cpp \
							desmume/draw.cpp 
							
LOCAL_ARM_MODE 			:= arm
LOCAL_ARM_NEON 			:= false
LOCAL_CPP_FEATURES		:= exceptions
LOCAL_CFLAGS			:= -DANDROID -DUSE_EGL -DUSE_GLES -DNO_MEMDEBUG -DHAVE_JIT -DHAVE_LIBAGG -DHAVE_LIBZ -DCOMPRESS_MT -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16
LOCAL_LDLIBS 			:= -llog -lz -lEGL -lGLESv1_CM -ljnigraphics -lOpenSLES -landroid -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 

LOCAL_CFLAGS += -DGFX3D_USE_FLOAT

#LOCAL_CFLAGS += -flto
#LOCAL_LDLIBS += -flto -fuse-ld=bfd -finline-limit=300 -Ofast -ftree-vectorize -fsingle-precision-constant -fprefetch-loop-arrays -fvariable-expansion-in-unroller -ffast-math -funroll-loops -fomit-frame-pointer -fstrict-aliasing -fno-math-errno -funsafe-math-optimizations -ffinite-math-only -ffunction-sections -fdata-sections -fbranch-target-load-optimize2 -fno-stack-protector -flto -fforce-addr -funswitch-loops -ftree-loop-im -ftree-loop-ivcanon -fivopts

include $(BUILD_SHARED_LIBRARY)
