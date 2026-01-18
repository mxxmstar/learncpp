FFmpeg 64-bit static Windows build from www.gyan.dev

Version: 2025-05-01-git-707c04fe06-full_build-www.gyan.dev

License: GPL v3

Source Code: https://github.com/FFmpeg/FFmpeg/commit/707c04fe06

External Assets
frei0r plugins:   https://www.gyan.dev/ffmpeg/builds/ffmpeg-frei0r-plugins
lensfun database: https://www.gyan.dev/ffmpeg/builds/ffmpeg-lensfun-db

git-full build configuration: 

ARCH                      x86 (generic)
big-endian                no
runtime cpu detection     yes
standalone assembly       yes
x86 assembler             nasm
MMX enabled               yes
MMXEXT enabled            yes
3DNow! enabled            yes
3DNow! extended enabled   yes
SSE enabled               yes
SSSE3 enabled             yes
AESNI enabled             yes
AVX enabled               yes
AVX2 enabled              yes
AVX-512 enabled           yes
AVX-512ICL enabled        yes
XOP enabled               yes
FMA3 enabled              yes
FMA4 enabled              yes
i686 features enabled     yes
CMOV is fast              yes
EBX available             yes
EBP available             yes
debug symbols             yes
strip symbols             yes
optimize for size         no
optimizations             yes
static                    yes
shared                    no
postprocessing support    yes
network support           yes
threading support         pthreads
safe bitstream reader     yes
texi2html enabled         no
perl enabled              yes
pod2man enabled           yes
makeinfo enabled          yes
makeinfo supports HTML    yes
xmllint enabled           yes

External libraries:
avisynth                libgsm                  libsvtav1
bzlib                   libharfbuzz             libtheora
chromaprint             libilbc                 libtwolame
frei0r                  libjxl                  libuavs3d
gmp                     liblc3                  libvidstab
gnutls                  liblensfun              libvmaf
iconv                   libmodplug              libvo_amrwbenc
ladspa                  libmp3lame              libvorbis
lcms2                   libmysofa               libvpx
libaom                  libopencore_amrnb       libvvenc
libaribb24              libopencore_amrwb       libwebp
libaribcaption          libopenjpeg             libx264
libass                  libopenmpt              libx265
libbluray               libopus                 libxavs2
libbs2b                 libplacebo              libxevd
libcaca                 libqrencode             libxeve
libcdio                 libquirc                libxml2
libcodec2               librav1e                libxvid
libdav1d                librist                 libzimg
libdavs2                librubberband           libzmq
libdvdnav               libshaderc              libzvbi
libdvdread              libshine                lzma
libflite                libsnappy               mediafoundation
libfontconfig           libsoxr                 sdl2
libfreetype             libspeex                zlib
libfribidi              libsrt
libgme                  libssh

External libraries providing hardware acceleration:
amf                     d3d12va                 nvdec
cuda                    dxva2                   nvenc
cuda_llvm               ffnvcodec               opencl
cuvid                   libmfx                  vaapi
d3d11va                 libvpl                  vulkan

Libraries:
avcodec                 avformat                swresample
avdevice                avutil                  swscale
avfilter                postproc

Programs:
ffmpeg                  ffplay                  ffprobe

Enabled decoders:
aac                     g2m                     pcm_u8
aac_fixed               g723_1                  pcm_vidc
aac_latm                g729                    pcx
aasc                    gdv                     pdv
ac3                     gem                     pfm
ac3_fixed               gif                     pgm
acelp_kelvin            gremlin_dpcm            pgmyuv
adpcm_4xm               gsm                     pgssub
adpcm_adx               gsm_ms                  pgx
adpcm_afc               h261                    phm
adpcm_agm               h263                    photocd
adpcm_aica              h263i                   pictor
adpcm_argo              h263p                   pixlet
adpcm_ct                h264                    pjs
adpcm_dtk               h264_amf                png
adpcm_ea                h264_cuvid              ppm
adpcm_ea_maxis_xa       h264_qsv                prores
adpcm_ea_r1             hap                     prosumer
adpcm_ea_r2             hca                     psd
adpcm_ea_r3             hcom                    ptx
adpcm_ea_xas            hdr                     qcelp
adpcm_g722              hevc                    qdm2
adpcm_g726              hevc_amf                qdmc
adpcm_g726le            hevc_cuvid              qdraw
adpcm_ima_acorn         hevc_qsv                qoa
adpcm_ima_alp           hnm4_video              qoi
adpcm_ima_amv           hq_hqa                  qpeg
adpcm_ima_apc           hqx                     qtrle
adpcm_ima_apm           huffyuv                 r10k
adpcm_ima_cunning       hymt                    r210
adpcm_ima_dat4          iac                     ra_144
adpcm_ima_dk3           idcin                   ra_288
adpcm_ima_dk4           idf                     ralf
adpcm_ima_ea_eacs       iff_ilbm                rasc
adpcm_ima_ea_sead       ilbc                    rawvideo
adpcm_ima_iss           imc                     realtext
adpcm_ima_moflex        imm4                    rka
adpcm_ima_mtf           imm5                    rl2
adpcm_ima_oki           indeo2                  roq
adpcm_ima_qt            indeo3                  roq_dpcm
adpcm_ima_rad           indeo4                  rpza
adpcm_ima_smjpeg        indeo5                  rscc
adpcm_ima_ssi           interplay_acm           rtv1
adpcm_ima_wav           interplay_dpcm          rv10
adpcm_ima_ws            interplay_video         rv20
adpcm_ima_xbox          ipu                     rv30
adpcm_ms                jacosub                 rv40
adpcm_mtaf              jpeg2000                rv60
adpcm_psx               jpegls                  s302m
adpcm_sbpro_2           jv                      sami
adpcm_sbpro_3           kgv1                    sanm
adpcm_sbpro_4           kmvc                    sbc
adpcm_swf               lagarith                scpr
adpcm_thp               lead                    screenpresso
adpcm_thp_le            libaom_av1              sdx2_dpcm
adpcm_vima              libaribb24              sga
adpcm_xa                libaribcaption          sgi
adpcm_xmd               libcodec2               sgirle
adpcm_yamaha            libdav1d                sheervideo
adpcm_zork              libdavs2                shorten
agm                     libgsm                  simbiosis_imx
aic                     libgsm_ms               sipr
alac                    libilbc                 siren
alias_pix               libjxl                  smackaud
als                     libjxl_anim             smacker
amrnb                   liblc3                  smc
amrwb                   libopencore_amrnb       smvjpeg
amv                     libopencore_amrwb       snow
anm                     libopus                 sol_dpcm
ansi                    libspeex                sonic
anull                   libuavs3d               sp5x
apac                    libvorbis               speedhq
ape                     libvpx_vp8              speex
apng                    libvpx_vp9              srgc
aptx                    libxevd                 srt
aptx_hd                 libzvbi_teletext        ssa
apv                     loco                    stl
arbc                    lscr                    subrip
argo                    m101                    subviewer
ass                     mace3                   subviewer1
asv1                    mace6                   sunrast
asv2                    magicyuv                svq1
atrac1                  mdec                    svq3
atrac3                  media100                tak
atrac3al                metasound               targa
atrac3p                 microdvd                targa_y216
atrac3pal               mimic                   tdsc
atrac9                  misc4                   text
aura                    mjpeg                   theora
aura2                   mjpeg_cuvid             thp
av1                     mjpeg_qsv               tiertexseqvideo
av1_amf                 mjpegb                  tiff
av1_cuvid               mlp                     tmv
av1_qsv                 mmvideo                 truehd
avrn                    mobiclip                truemotion1
avrp                    motionpixels            truemotion2
avs                     movtext                 truemotion2rt
avui                    mp1                     truespeech
bethsoftvid             mp1float                tscc
bfi                     mp2                     tscc2
bink                    mp2float                tta
binkaudio_dct           mp3                     twinvq
binkaudio_rdft          mp3adu                  txd
bintext                 mp3adufloat             ulti
bitpacked               mp3float                utvideo
bmp                     mp3on4                  v210
bmv_audio               mp3on4float             v210x
bmv_video               mpc7                    v308
bonk                    mpc8                    v408
brender_pix             mpeg1_cuvid             v410
c93                     mpeg1video              vb
cavs                    mpeg2_cuvid             vble
cbd2_dpcm               mpeg2_qsv               vbn
ccaption                mpeg2video              vc1
cdgraphics              mpeg4                   vc1_cuvid
cdtoons                 mpeg4_cuvid             vc1_qsv
cdxl                    mpegvideo               vc1image
cfhd                    mpl2                    vcr1
cinepak                 msa1                    vmdaudio
clearvideo              mscc                    vmdvideo
cljr                    msmpeg4v1               vmix
cllc                    msmpeg4v2               vmnc
comfortnoise            msmpeg4v3               vnull
cook                    msnsiren                vorbis
cpia                    msp2                    vp3
cri                     msrle                   vp4
cscd                    mss1                    vp5
cyuv                    mss2                    vp6
dca                     msvideo1                vp6a
dds                     mszh                    vp6f
derf_dpcm               mts2                    vp7
dfa                     mv30                    vp8
dfpwm                   mvc1                    vp8_cuvid
dirac                   mvc2                    vp8_qsv
dnxhd                   mvdv                    vp9
dolby_e                 mvha                    vp9_cuvid
dpx                     mwsc                    vp9_qsv
dsd_lsbf                mxpeg                   vplayer
dsd_lsbf_planar         nellymoser              vqa
dsd_msbf                notchlc                 vqc
dsd_msbf_planar         nuv                     vvc
dsicinaudio             on2avc                  vvc_qsv
dsicinvideo             opus                    wady_dpcm
dss_sp                  osq                     wavarc
dst                     paf_audio               wavpack
dvaudio                 paf_video               wbmp
dvbsub                  pam                     wcmv
dvdsub                  pbm                     webp
dvvideo                 pcm_alaw                webvtt
dxa                     pcm_bluray              wmalossless
dxtory                  pcm_dvd                 wmapro
dxv                     pcm_f16le               wmav1
eac3                    pcm_f24le               wmav2
eacmv                   pcm_f32be               wmavoice
eamad                   pcm_f32le               wmv1
eatgq                   pcm_f64be               wmv2
eatgv                   pcm_f64le               wmv3
eatqi                   pcm_lxf                 wmv3image
eightbps                pcm_mulaw               wnv1
eightsvx_exp            pcm_s16be               wrapped_avframe
eightsvx_fib            pcm_s16be_planar        ws_snd1
escape124               pcm_s16le               xan_dpcm
escape130               pcm_s16le_planar        xan_wc3
evrc                    pcm_s24be               xan_wc4
exr                     pcm_s24daud             xbin
fastaudio               pcm_s24le               xbm
ffv1                    pcm_s24le_planar        xface
ffvhuff                 pcm_s32be               xl
ffwavesynth             pcm_s32le               xma1
fic                     pcm_s32le_planar        xma2
fits                    pcm_s64be               xpm
flac                    pcm_s64le               xsub
flashsv                 pcm_s8                  xwd
flashsv2                pcm_s8_planar           y41p
flic                    pcm_sga                 ylc
flv                     pcm_u16be               yop
fmvc                    pcm_u16le               yuv4
fourxm                  pcm_u24be               zero12v
fraps                   pcm_u24le               zerocodec
frwu                    pcm_u32be               zlib
ftr                     pcm_u32le               zmbv

Enabled encoders:
a64multi                hevc_nvenc              pcm_s32le_planar
a64multi5               hevc_qsv                pcm_s64be
aac                     hevc_vaapi              pcm_s64le
aac_mf                  hevc_vulkan             pcm_s8
ac3                     huffyuv                 pcm_s8_planar
ac3_fixed               jpeg2000                pcm_u16be
ac3_mf                  jpegls                  pcm_u16le
adpcm_adx               libaom_av1              pcm_u24be
adpcm_argo              libcodec2               pcm_u24le
adpcm_g722              libgsm                  pcm_u32be
adpcm_g726              libgsm_ms               pcm_u32le
adpcm_g726le            libilbc                 pcm_u8
adpcm_ima_alp           libjxl                  pcm_vidc
adpcm_ima_amv           libjxl_anim             pcx
adpcm_ima_apm           liblc3                  pfm
adpcm_ima_qt            libmp3lame              pgm
adpcm_ima_ssi           libopencore_amrnb       pgmyuv
adpcm_ima_wav           libopenjpeg             phm
adpcm_ima_ws            libopus                 png
adpcm_ms                librav1e                ppm
adpcm_swf               libshine                prores
adpcm_yamaha            libspeex                prores_aw
alac                    libsvtav1               prores_ks
alias_pix               libtheora               qoi
amv                     libtwolame              qtrle
anull                   libvo_amrwbenc          r10k
apng                    libvorbis               r210
aptx                    libvpx_vp8              ra_144
aptx_hd                 libvpx_vp9              rawvideo
ass                     libvvenc                roq
asv1                    libwebp                 roq_dpcm
asv2                    libwebp_anim            rpza
av1_amf                 libx264                 rv10
av1_mf                  libx264rgb              rv20
av1_nvenc               libx265                 s302m
av1_qsv                 libxavs2                sbc
av1_vaapi               libxeve                 sgi
avrp                    libxvid                 smc
avui                    ljpeg                   snow
bitpacked               magicyuv                speedhq
bmp                     mjpeg                   srt
cfhd                    mjpeg_qsv               ssa
cinepak                 mjpeg_vaapi             subrip
cljr                    mlp                     sunrast
comfortnoise            movtext                 svq1
dca                     mp2                     targa
dfpwm                   mp2fixed                text
dnxhd                   mp3_mf                  tiff
dpx                     mpeg1video              truehd
dvbsub                  mpeg2_qsv               tta
dvdsub                  mpeg2_vaapi             ttml
dvvideo                 mpeg2video              utvideo
dxv                     mpeg4                   v210
eac3                    msmpeg4v2               v308
exr                     msmpeg4v3               v408
ffv1                    msrle                   v410
ffv1_vulkan             msvideo1                vbn
ffvhuff                 nellymoser              vc2
fits                    opus                    vnull
flac                    pam                     vorbis
flashsv                 pbm                     vp8_vaapi
flashsv2                pcm_alaw                vp9_qsv
flv                     pcm_bluray              vp9_vaapi
g723_1                  pcm_dvd                 wavpack
gif                     pcm_f32be               wbmp
h261                    pcm_f32le               webvtt
h263                    pcm_f64be               wmav1
h263p                   pcm_f64le               wmav2
h264_amf                pcm_mulaw               wmv1
h264_mf                 pcm_s16be               wmv2
h264_nvenc              pcm_s16be_planar        wrapped_avframe
h264_qsv                pcm_s16le               xbm
h264_vaapi              pcm_s16le_planar        xface
h264_vulkan             pcm_s24be               xsub
hap                     pcm_s24daud             xwd
hdr                     pcm_s24le               y41p
hevc_amf                pcm_s24le_planar        yuv4
hevc_d3d12va            pcm_s32be               zlib
hevc_mf                 pcm_s32le               zmbv

Enabled hwaccels:
av1_d3d11va             hevc_dxva2              vc1_nvdec
av1_d3d11va2            hevc_nvdec              vc1_vaapi
av1_d3d12va             hevc_vaapi              vp8_nvdec
av1_dxva2               hevc_vulkan             vp8_vaapi
av1_nvdec               mjpeg_nvdec             vp9_d3d11va
av1_vaapi               mjpeg_vaapi             vp9_d3d11va2
av1_vulkan              mpeg1_nvdec             vp9_d3d12va
ffv1_vulkan             mpeg2_d3d11va           vp9_dxva2
h263_vaapi              mpeg2_d3d11va2          vp9_nvdec
h264_d3d11va            mpeg2_d3d12va           vp9_vaapi
h264_d3d11va2           mpeg2_dxva2             vvc_vaapi
h264_d3d12va            mpeg2_nvdec             wmv3_d3d11va
h264_dxva2              mpeg2_vaapi             wmv3_d3d11va2
h264_nvdec              mpeg4_nvdec             wmv3_d3d12va
h264_vaapi              mpeg4_vaapi             wmv3_dxva2
h264_vulkan             vc1_d3d11va             wmv3_nvdec
hevc_d3d11va            vc1_d3d11va2            wmv3_vaapi
hevc_d3d11va2           vc1_d3d12va
hevc_d3d12va            vc1_dxva2

Enabled parsers:
aac                     dvdsub                  mpegaudio
aac_latm                evc                     mpegvideo
ac3                     ffv1                    opus
adx                     flac                    png
amr                     ftr                     pnm
av1                     g723_1                  qoi
avs2                    g729                    rv34
avs3                    gif                     sbc
bmp                     gsm                     sipr
cavsvideo               h261                    tak
cook                    h263                    vc1
cri                     h264                    vorbis
dca                     hdr                     vp3
dirac                   hevc                    vp8
dnxhd                   ipu                     vp9
dnxuc                   jpeg2000                vvc
dolby_e                 jpegxl                  webp
dpx                     misc4                   xbm
dvaudio                 mjpeg                   xma
dvbsub                  mlp                     xwd
dvd_nav                 mpeg4video

Enabled demuxers:
aa                      idcin                   pcm_f64le
aac                     idf                     pcm_mulaw
aax                     iff                     pcm_s16be
ac3                     ifv                     pcm_s16le
ac4                     ilbc                    pcm_s24be
ace                     image2                  pcm_s24le
acm                     image2_alias_pix        pcm_s32be
act                     image2_brender_pix      pcm_s32le
adf                     image2pipe              pcm_s8
adp                     image_bmp_pipe          pcm_u16be
ads                     image_cri_pipe          pcm_u16le
adx                     image_dds_pipe          pcm_u24be
aea                     image_dpx_pipe          pcm_u24le
afc                     image_exr_pipe          pcm_u32be
aiff                    image_gem_pipe          pcm_u32le
aix                     image_gif_pipe          pcm_u8
alp                     image_hdr_pipe          pcm_vidc
amr                     image_j2k_pipe          pdv
amrnb                   image_jpeg_pipe         pjs
amrwb                   image_jpegls_pipe       pmp
anm                     image_jpegxl_pipe       pp_bnk
apac                    image_pam_pipe          pva
apc                     image_pbm_pipe          pvf
ape                     image_pcx_pipe          qcp
apm                     image_pfm_pipe          qoa
apng                    image_pgm_pipe          r3d
aptx                    image_pgmyuv_pipe       rawvideo
aptx_hd                 image_pgx_pipe          rcwt
apv                     image_phm_pipe          realtext
aqtitle                 image_photocd_pipe      redspark
argo_asf                image_pictor_pipe       rka
argo_brp                image_png_pipe          rl2
argo_cvg                image_ppm_pipe          rm
asf                     image_psd_pipe          roq
asf_o                   image_qdraw_pipe        rpl
ass                     image_qoi_pipe          rsd
ast                     image_sgi_pipe          rso
au                      image_sunrast_pipe      rtp
av1                     image_svg_pipe          rtsp
avi                     image_tiff_pipe         s337m
avisynth                image_vbn_pipe          sami
avr                     image_webp_pipe         sap
avs                     image_xbm_pipe          sbc
avs2                    image_xpm_pipe          sbg
avs3                    image_xwd_pipe          scc
bethsoftvid             imf                     scd
bfi                     ingenient               sdns
bfstm                   ipmovie                 sdp
bink                    ipu                     sdr2
binka                   ircam                   sds
bintext                 iss                     sdx
bit                     iv8                     segafilm
bitpacked               ivf                     ser
bmv                     ivr                     sga
boa                     jacosub                 shorten
bonk                    jpegxl_anim             siff
brstm                   jv                      simbiosis_imx
c93                     kux                     sln
caf                     kvag                    smacker
cavsvideo               laf                     smjpeg
cdg                     lc3                     smush
cdxl                    libgme                  sol
cine                    libmodplug              sox
codec2                  libopenmpt              spdif
codec2raw               live_flv                srt
concat                  lmlm4                   stl
dash                    loas                    str
data                    lrc                     subviewer
daud                    luodat                  subviewer1
dcstr                   lvf                     sup
derf                    lxf                     svag
dfa                     m4v                     svs
dfpwm                   matroska                swf
dhav                    mca                     tak
dirac                   mcc                     tedcaptions
dnxhd                   mgsts                   thp
dsf                     microdvd                threedostr
dsicin                  mjpeg                   tiertexseq
dss                     mjpeg_2000              tmv
dts                     mlp                     truehd
dtshd                   mlv                     tta
dv                      mm                      tty
dvbsub                  mmf                     txd
dvbtxt                  mods                    ty
dvdvideo                moflex                  usm
dxa                     mov                     v210
ea                      mp3                     v210x
ea_cdata                mpc                     vag
eac3                    mpc8                    vc1
epaf                    mpegps                  vc1t
evc                     mpegts                  vividas
ffmetadata              mpegtsraw               vivo
filmstrip               mpegvideo               vmd
fits                    mpjpeg                  vobsub
flac                    mpl2                    voc
flic                    mpsub                   vpk
flv                     msf                     vplayer
fourxm                  msnwc_tcp               vqf
frm                     msp                     vvc
fsb                     mtaf                    w64
fwse                    mtv                     wady
g722                    musx                    wav
g723_1                  mv                      wavarc
g726                    mvi                     wc3
g726le                  mxf                     webm_dash_manifest
g729                    mxg                     webvtt
gdv                     nc                      wsaud
genh                    nistsphere              wsd
gif                     nsp                     wsvqa
gsm                     nsv                     wtv
gxf                     nut                     wv
h261                    nuv                     wve
h263                    obu                     xa
h264                    ogg                     xbin
hca                     oma                     xmd
hcom                    osq                     xmv
hevc                    paf                     xvag
hls                     pcm_alaw                xwma
hnm                     pcm_f32be               yop
iamf                    pcm_f32le               yuv4mpegpipe
ico                     pcm_f64be

Enabled muxers:
a64                     h261                    pcm_s16le
ac3                     h263                    pcm_s24be
ac4                     h264                    pcm_s24le
adts                    hash                    pcm_s32be
adx                     hds                     pcm_s32le
aea                     hevc                    pcm_s8
aiff                    hls                     pcm_u16be
alp                     iamf                    pcm_u16le
amr                     ico                     pcm_u24be
amv                     ilbc                    pcm_u24le
apm                     image2                  pcm_u32be
apng                    image2pipe              pcm_u32le
aptx                    ipod                    pcm_u8
aptx_hd                 ircam                   pcm_vidc
apv                     ismv                    psp
argo_asf                ivf                     rawvideo
argo_cvg                jacosub                 rcwt
asf                     kvag                    rm
asf_stream              latm                    roq
ass                     lc3                     rso
ast                     lrc                     rtp
au                      m4v                     rtp_mpegts
avi                     matroska                rtsp
avif                    matroska_audio          sap
avm2                    md5                     sbc
avs2                    microdvd                scc
avs3                    mjpeg                   segafilm
bit                     mkvtimestamp_v2         segment
caf                     mlp                     smjpeg
cavsvideo               mmf                     smoothstreaming
chromaprint             mov                     sox
codec2                  mp2                     spdif
codec2raw               mp3                     spx
crc                     mp4                     srt
dash                    mpeg1system             stream_segment
data                    mpeg1vcd                streamhash
daud                    mpeg1video              sup
dfpwm                   mpeg2dvd                swf
dirac                   mpeg2svcd               tee
dnxhd                   mpeg2video              tg2
dts                     mpeg2vob                tgp
dv                      mpegts                  truehd
eac3                    mpjpeg                  tta
evc                     mxf                     ttml
f4v                     mxf_d10                 uncodedframecrc
ffmetadata              mxf_opatom              vc1
fifo                    null                    vc1t
filmstrip               nut                     voc
fits                    obu                     vvc
flac                    oga                     w64
flv                     ogg                     wav
framecrc                ogv                     webm
framehash               oma                     webm_chunk
framemd5                opus                    webm_dash_manifest
g722                    pcm_alaw                webp
g723_1                  pcm_f32be               webvtt
g726                    pcm_f32le               wsaud
g726le                  pcm_f64be               wtv
gif                     pcm_f64le               wv
gsm                     pcm_mulaw               yuv4mpegpipe
gxf                     pcm_s16be

Enabled protocols:
async                   http                    rtmp
bluray                  httpproxy               rtmpe
cache                   https                   rtmps
concat                  icecast                 rtmpt
concatf                 ipfs_gateway            rtmpte
crypto                  ipns_gateway            rtmpts
data                    librist                 rtp
fd                      libsrt                  srtp
ffrtmpcrypt             libssh                  subfile
ffrtmphttp              libzmq                  tcp
file                    md5                     tee
ftp                     mmsh                    tls
gopher                  mmst                    udp
gophers                 pipe                    udplite
hls                     prompeg

Enabled filters:
a3dscope                deblock                 perlin
aap                     decimate                perms
abench                  deconvolve              perspective
abitscope               dedot                   phase
acompressor             deesser                 photosensitivity
acontrast               deflate                 pixdesctest
acopy                   deflicker               pixelize
acrossfade              deinterlace_qsv         pixscope
acrossover              deinterlace_vaapi       pp
acrusher                dejudder                pp7
acue                    delogo                  premultiply
addroi                  denoise_vaapi           prewitt
adeclick                deshake                 prewitt_opencl
adeclip                 deshake_opencl          procamp_vaapi
adecorrelate            despill                 program_opencl
adelay                  detelecine              pseudocolor
adenorm                 dialoguenhance          psnr
aderivative             dilation                pullup
adrawgraph              dilation_opencl         qp
adrc                    displace                qrencode
adynamicequalizer       doubleweave             qrencodesrc
adynamicsmooth          drawbox                 quirc
aecho                   drawbox_vaapi           random
aemphasis               drawgraph               readeia608
aeval                   drawgrid                readvitc
aevalsrc                drawtext                realtime
aexciter                drmeter                 remap
afade                   dynaudnorm              remap_opencl
afdelaysrc              earwax                  removegrain
afftdn                  ebur128                 removelogo
afftfilt                edgedetect              repeatfields
afir                    elbg                    replaygain
afireqsrc               entropy                 reverse
afirsrc                 epx                     rgbashift
aformat                 eq                      rgbtestsrc
afreqshift              equalizer               roberts
afwtdn                  erosion                 roberts_opencl
agate                   erosion_opencl          rotate
agraphmonitor           estdif                  rubberband
ahistogram              exposure                sab
aiir                    extractplanes           scale
aintegral               extrastereo             scale2ref
ainterleave             fade                    scale_cuda
alatency                feedback                scale_qsv
alimiter                fftdnoiz                scale_vaapi
allpass                 fftfilt                 scale_vulkan
allrgb                  field                   scdet
allyuv                  fieldhint               scharr
aloop                   fieldmatch              scroll
alphaextract            fieldorder              segment
alphamerge              fillborders             select
amerge                  find_rect               selectivecolor
ametadata               firequalizer            sendcmd
amix                    flanger                 separatefields
amovie                  flip_vulkan             setdar
amplify                 flite                   setfield
amultiply               floodfill               setparams
anequalizer             format                  setpts
anlmdn                  fps                     setrange
anlmf                   framepack               setsar
anlms                   framerate               settb
anoisesrc               framestep               sharpness_vaapi
anull                   freezedetect            shear
anullsink               freezeframes            showcqt
anullsrc                frei0r                  showcwt
apad                    frei0r_src              showfreqs
aperms                  fspp                    showinfo
aphasemeter             fsync                   showpalette
aphaser                 gblur                   showspatial
aphaseshift             gblur_vulkan            showspectrum
apsnr                   geq                     showspectrumpic
apsyclip                gradfun                 showvolume
apulsator               gradients               showwaves
arealtime               graphmonitor            showwavespic
aresample               grayworld               shuffleframes
areverse                greyedge                shufflepixels
arls                    guided                  shuffleplanes
arnndn                  haas                    sidechaincompress
asdr                    haldclut                sidechaingate
asegment                haldclutsrc             sidedata
aselect                 hdcd                    sierpinski
asendcmd                headphone               signalstats
asetnsamples            hflip                   signature
asetpts                 hflip_vulkan            silencedetect
asetrate                highpass                silenceremove
asettb                  highshelf               sinc
ashowinfo               hilbert                 sine
asidedata               histeq                  siti
asisdr                  histogram               smartblur
asoftclip               hqdn3d                  smptebars
aspectralstats          hqx                     smptehdbars
asplit                  hstack                  sobel
ass                     hstack_qsv              sobel_opencl
astats                  hstack_vaapi            sofalizer
astreamselect           hsvhold                 spectrumsynth
asubboost               hsvkey                  speechnorm
asubcut                 hue                     split
asupercut               huesaturation           spp
asuperpass              hwdownload              sr_amf
asuperstop              hwmap                   ssim
atadenoise              hwupload                ssim360
atempo                  hwupload_cuda           stereo3d
atilt                   hysteresis              stereotools
atrim                   iccdetect               stereowiden
avectorscope            iccgen                  streamselect
avgblur                 identity                subtitles
avgblur_opencl          idet                    super2xsai
avgblur_vulkan          il                      superequalizer
avsynctest              inflate                 surround
axcorrelate             interlace               swaprect
azmq                    interlace_vulkan        swapuv
backgroundkey           interleave              tblend
bandpass                join                    telecine
bandreject              kerndeint               testsrc
bass                    kirsch                  testsrc2
bbox                    ladspa                  thistogram
bench                   lagfun                  threshold
bilateral               latency                 thumbnail
bilateral_cuda          lenscorrection          thumbnail_cuda
biquad                  lensfun                 tile
bitplanenoise           libplacebo              tiltandshift
blackdetect             libvmaf                 tiltshelf
blackframe              life                    tinterlace
blend                   limitdiff               tlut2
blend_vulkan            limiter                 tmedian
blockdetect             loop                    tmidequalizer
blurdetect              loudnorm                tmix
bm3d                    lowpass                 tonemap
boxblur                 lowshelf                tonemap_opencl
boxblur_opencl          lumakey                 tonemap_vaapi
bs2b                    lut                     tpad
bwdif                   lut1d                   transpose
bwdif_cuda              lut2                    transpose_opencl
bwdif_vulkan            lut3d                   transpose_vaapi
cas                     lutrgb                  transpose_vulkan
ccrepack                lutyuv                  treble
cellauto                mandelbrot              tremolo
channelmap              maskedclamp             trim
channelsplit            maskedmax               unpremultiply
chorus                  maskedmerge             unsharp
chromaber_vulkan        maskedmin               unsharp_opencl
chromahold              maskedthreshold         untile
chromakey               maskfun                 uspp
chromakey_cuda          mcdeint                 v360
chromanr                mcompand                vaguedenoiser
chromashift             median                  varblur
ciescope                mergeplanes             vectorscope
codecview               mestimate               vflip
color                   metadata                vflip_vulkan
color_vulkan            midequalizer            vfrdet
colorbalance            minterpolate            vibrance
colorchannelmixer       mix                     vibrato
colorchart              monochrome              vidstabdetect
colorcontrast           morpho                  vidstabtransform
colorcorrect            movie                   vif
colorhold               mpdecimate              vignette
colorize                mptestsrc               virtualbass
colorkey                msad                    vmafmotion
colorkey_opencl         multiply                volume
colorlevels             negate                  volumedetect
colormap                nlmeans                 vpp_amf
colormatrix             nlmeans_opencl          vpp_qsv
colorspace              nlmeans_vulkan          vstack
colorspace_cuda         nnedi                   vstack_qsv
colorspectrum           noformat                vstack_vaapi
colortemperature        noise                   w3fdif
compand                 normalize               waveform
compensationdelay       null                    weave
concat                  nullsink                xbr
convolution             nullsrc                 xcorrelate
convolution_opencl      openclsrc               xfade
convolve                oscilloscope            xfade_opencl
copy                    overlay                 xfade_vulkan
corr                    overlay_cuda            xmedian
cover_rect              overlay_opencl          xpsnr
crop                    overlay_qsv             xstack
cropdetect              overlay_vaapi           xstack_qsv
crossfeed               overlay_vulkan          xstack_vaapi
crystalizer             owdenoise               yadif
cue                     pad                     yadif_cuda
curves                  pad_opencl              yaepblur
datascope               pad_vaapi               yuvtestsrc
dblur                   pal100bars              zmq
dcshift                 pal75bars               zoneplate
dctdnoiz                palettegen              zoompan
ddagrab                 paletteuse              zscale
deband                  pan

Enabled bsfs:
aac_adtstoasc           h264_mp4toannexb        pgs_frame_merge
apv_metadata            h264_redundant_pps      prores_metadata
av1_frame_merge         hapqa_extract           remove_extradata
av1_frame_split         hevc_metadata           setts
av1_metadata            hevc_mp4toannexb        showinfo
chomp                   imx_dump_header         text2movsub
dca_core                media100_to_mjpegb      trace_headers
dovi_rpu                mjpeg2jpeg              truehd_core
dts2pts                 mjpega_dump_header      vp9_metadata
dump_extradata          mov2textsub             vp9_raw_reorder
dv_error_marker         mpeg2_metadata          vp9_superframe
eac3_core               mpeg4_unpack_bframes    vp9_superframe_split
evc_frame_merge         noise                   vvc_metadata
extract_extradata       null                    vvc_mp4toannexb
filter_units            opus_metadata
h264_metadata           pcm_rechunk

Enabled indevs:
dshow                   lavfi                   vfwcap
gdigrab                 libcdio

Enabled outdevs:
caca

git-full external libraries' versions: 

AMF v1.4.36.0-2-gd7311e3
aom v3.12.1-114-g719f60edc5
aribb24 v1.0.3-5-g5e9be27
aribcaption 1.1.1
AviSynthPlus v3.7.5-10-g0af83bfb
bs2b 3.1.0
chromaprint 1.5.1
codec2 1.2.0-106-g96e8a19
dav1d 1.5.1-5-g8d95618
davs2 1.7-1-gb41cf11
dvdnav 6.1.1-23-g9831fe0
dvdread 6.1.3-15-g786e735
ffnvcodec n13.0.19.0-1-gf2fb9b3
flite v2.2-55-g6c9f20d
freetype VER-2-13-3
frei0r v2.3.3-15-gb47c180
fribidi v1.0.16-2-gb28f43b
gsm 1.0.22
harfbuzz 11.2.0
ladspa-sdk 1.17
lame 3.100
lc3 1.1.3
lcms2 2.16
lensfun v0.3.95-1670-g427754bd
libass 0.17.3-81-g6955093
libcdio-paranoia 10.2
libgme 0.6.3
libilbc v3.0.4-346-g6adb26d4a4
libjxl v0.11-snapshot-241-g03efbf9a
libopencore-amrnb 0.1.6
libopencore-amrwb 0.1.6
libplacebo v7.349.0-65-g2bd627f8
libsoxr 0.1.3
libssh 0.11.1
libtheora 1.1.1
libwebp v1.5.0-42-geb3ff781
OpenCL-Headers v2024.10.24-7-g265df85
openmpt libopenmpt-0.6.22-24-g821723f93
opus v1.5.2-105-ga3890352
qrencode 4.1.1
quirc 1.2
rav1e p20250225-20-gcda12985
rist 0.2.12
rubberband v1.8.1
SDL release-2.32.0-40-g15fd3fcdc
shaderc v2025.2-1-ga5a8caa
shine 3.1.1
snappy 1.2.2
speex Speex-1.2.1-47-ga145463
srt v1.5.4-31-g952f9495
SVT-AV1 v3.0.2-41-g8df04456
twolame 0.4.0
uavs3d v1.1-47-g1fd0491
VAAPI 2.23.0.
vidstab v1.1.1-14-gd2d55a8
vmaf v3.0.0-108-g8b2582db
vo-amrwbenc 0.1.3
vorbis v1.3.7-10-g84c02369
VPL 2.15
vpx v1.15.1-79-gb32e98f52
vulkan-loader v1.4.313-4-g2526eacfc
vvenc v1.13.1-28-gf8865a5
x264 v0.165.3215
x265 4.1-144-g9d6b685
xavs2 1.4
xevd 0.5.0
xeve 0.5.1
xvid v1.3.7
zeromq 4.3.5
zimg release-3.0.5-202-g3eea036
zvbi v0.2.44

