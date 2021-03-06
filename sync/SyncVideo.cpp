
#include "SyncVideo.h"

volatile sig_atomic_t SyncVideo::g_abort = false;
struct termios SyncVideo::orig_termios;
int SyncVideo::orig_fl;

bool omxplayer_remap_wanted(void)
{
  return false;
}


SyncVideo::SyncVideo()
{}

SyncVideo::~SyncVideo()
{}


SyncVideo::SyncVideo(const SyncVideo& vid)
{
  Init(vid.m_filename, vid.m_dateStart, vid.m_dateEnd, 
    vid.m_wallWidth, vid.m_wallHeight, vid.m_tileWidth, vid.m_tileHeight, vid.m_tileX, vid.m_tileY);
}

SyncVideo::SyncVideo(std::string path, time_t dateStart, time_t dateEnd, 
  float wallWidth, float wallHeight, float tileWidth, float tileHeight, 
  float tileX, float tileY, bool loop)
{
  Init(path, dateStart, dateEnd, wallWidth, wallHeight, tileWidth, tileHeight, tileX, tileY, loop);
}




void SyncVideo::Init(std::string path, time_t dateStart, time_t dateEnd, 
  float wallWidth, float wallHeight, float tileWidth, float tileHeight, 
  float tileX, float tileY, bool loop)
{
  m_filename = path;
  m_dateStart = dateStart;
  m_dateEnd = dateEnd;

  m_wallWidth = wallWidth;
  m_wallHeight = wallHeight;

  m_tileWidth = tileWidth;
  m_tileHeight = tileHeight;

  m_tileX = tileX;
  m_tileY = tileY;

  m_loop = loop;
}
void SyncVideo::Process()
{
  CLog::Log(1, "Process");
  // cout << "test video" << endl; 
  play();
}


void SyncVideo::Stop()
{
  m_stop = true;
  // SyncVideo::g_abort = true;
}

bool SyncVideo::isOver()
{
  return m_stop;
}

void SyncVideo::Run()
{
  Create();
}

SyncMedia::TypeMedia SyncVideo::GetType()
{
  return TYPE_VIDEO;
}

void SyncVideo::restore_termios()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}


void SyncVideo::restore_fl()
{
  fcntl(STDIN_FILENO, F_SETFL, orig_fl);
}
//#endif /* WANT_KEYS */

// void sig_handler(int s)
// {
//   printf("strg-c catched\n");
//   signal(SIGINT, SIG_DFL);
//   SyncVideo::g_abort = true;
// }

void SyncVideo::print_usage()
{
  printf("Usage: pwomxplayer [OPTIONS] [FILE]\n");
  printf("Options :\n");
  printf("         -h / --help                    print this help\n");
//  printf("         -a / --alang language          audio language        : e.g. ger\n");
  printf("         -n / --aidx  index             audio stream index    : e.g. 1\n");
  printf("         -o / --adev  device            audio out device      : e.g. hdmi/local\n");
  printf("         -i / --info                    dump stream format and exit\n");
  printf("         -q / --quiet                   suppress info messages\n");
  printf("         -s / --stats                   pts and buffer stats\n");
  printf("         -p / --passthrough             audio passthrough\n");
  printf("         -d / --deinterlace             deinterlacing\n");
  printf("         -w / --hw                      hw audio decoding\n");
  printf("         -3 / --3d mode                 switch tv into 3d mode (e.g. SBS/TB)\n");
  printf("         -y / --hdmiclocksync           adjust display refresh rate to match video (default)\n");
  printf("         -z / --nohdmiclocksync         do not adjust display refresh rate to match video\n");
  printf("         -t / --sid index               show subtitle with index\n");
  printf("         -r / --refresh                 adjust framerate/resolution to video\n");
  printf("         -l / --pos                     start position (in seconds)\n");  
  printf("              --boost-on-downmix        boost volume when downmixing\n");
  printf("              --vol                     Set initial volume in millibels (default 0)\n");
  printf("              --subtitles path          external subtitles in UTF-8 srt format\n");
  printf("              --font path               subtitle font\n");
  printf("                                        (default: /usr/share/fonts/truetype/freefont/FreeSans.ttf)\n");
  printf("              --font-size size          font size as thousandths of screen height\n");
  printf("                                        (default: 55)\n");
  printf("              --align left/center       subtitle alignment (default: left)\n");
  printf("              --lines n                 number of lines to accommodate in the subtitle buffer\n");
  printf("                                        (default: 3)\n");
  printf("              --win \"x1 y1 x2 y2\"       Set position of video window\n");
  printf("              --tile-code  code         use predefined tile position\n");
  printf("              --frame-size  FXxFY       set relative size of screen frame\n");
  printf("         -W / --wall WxH+L+T            video wall dimensions\n");
  printf("         -T / --tile WxH+L+T            video tile dimensions\n");
  printf("         -O / --orient <orient>         screen orientation\n");
  printf("         -F / --fit <fit>               how to handle aspect ratio\n");
  printf("         -A / --autotile                use .piwall definitions\n");
  printf("         -R / --role <role>             use role in .piwall\n");
  printf("         -C / --config <config>         use config in .piwall\n");
}

void SyncVideo::PrintSubtitleInfo()
{
  auto count = m_omx_reader.SubtitleStreamCount();
  size_t index = 0;

  if(m_has_external_subtitles)
  {
    ++count;
    if(!m_player_subtitles.GetUseExternalSubtitles())
      index = m_player_subtitles.GetActiveStream() + 1;
  }
  else if(m_has_subtitle)
  {
      index = m_player_subtitles.GetActiveStream();
  }

  printf("Subtitle count: %d, state: %s, index: %d, delay: %d\n",
         count,
         m_has_subtitle && m_player_subtitles.GetVisible() ? " on" : "off",
         index+1,
         m_has_subtitle ? m_player_subtitles.GetDelay() : 0);
}

void SyncVideo::SetSpeed(int iSpeed)
{
  if(!m_av_clock)
    return;

  if(iSpeed < OMX_PLAYSPEED_PAUSE)
    return;

  m_omx_reader.SetSpeed(iSpeed);

  if(m_av_clock->OMXPlaySpeed() != OMX_PLAYSPEED_PAUSE && iSpeed == OMX_PLAYSPEED_PAUSE)
    m_Pause = true;
  else if(m_av_clock->OMXPlaySpeed() == OMX_PLAYSPEED_PAUSE && iSpeed != OMX_PLAYSPEED_PAUSE)
    m_Pause = false;

  m_av_clock->OMXSpeed(iSpeed);
}

float SyncVideo::get_display_aspect_ratio(HDMI_ASPECT_T aspect)
{
  float display_aspect;
  switch (aspect) {
    case HDMI_ASPECT_4_3:   display_aspect = 4.0/3.0;   break;
    case HDMI_ASPECT_14_9:  display_aspect = 14.0/9.0;  break;
    case HDMI_ASPECT_16_9:  display_aspect = 16.0/9.0;  break;
    case HDMI_ASPECT_5_4:   display_aspect = 5.0/4.0;   break;
    case HDMI_ASPECT_16_10: display_aspect = 16.0/10.0; break;
    case HDMI_ASPECT_15_9:  display_aspect = 15.0/9.0;  break;
    case HDMI_ASPECT_64_27: display_aspect = 64.0/27.0; break;
    default:                display_aspect = 16.0/9.0;  break;
  }
  return display_aspect;
}

float SyncVideo::get_display_aspect_ratio(SDTV_ASPECT_T aspect)
{
  float display_aspect;
  switch (aspect) {
    case SDTV_ASPECT_4_3:  display_aspect = 4.0/3.0;  break;
    case SDTV_ASPECT_14_9: display_aspect = 14.0/9.0; break;
    case SDTV_ASPECT_16_9: display_aspect = 16.0/9.0; break;
    default:               display_aspect = 4.0/3.0;  break;
  }
  return display_aspect;
}

void SyncVideo::FlushStreams(double pts)
{
//  if(m_av_clock)
//    m_av_clock->OMXPause();

  if(m_has_video)
    m_player_video.Flush();

  if(m_has_audio)
    m_player_audio.Flush();

  if(m_has_subtitle)
    m_player_subtitles.Flush(pts);

  if(m_omx_pkt)
  {
    m_omx_reader.FreePacket(m_omx_pkt);
    m_omx_pkt = NULL;
  }

  if(pts != DVD_NOPTS_VALUE)
    m_av_clock->OMXUpdateClock(pts);

//  if(m_av_clock)
//  {
//    m_av_clock->OMXReset();
//    m_av_clock->OMXResume();
//  }
}

void SyncVideo::SetVideoMode(int width, int height, int fpsrate, int fpsscale, FORMAT_3D_T is3d, bool dump_mode)
{
  int32_t num_modes = 0;
  int i;
  HDMI_RES_GROUP_T prefer_group;
  HDMI_RES_GROUP_T group = HDMI_RES_GROUP_CEA;
  float fps = 60.0f; // better to force to higher rate if no information is known
  uint32_t prefer_mode;

  if (fpsrate && fpsscale)
    fps = DVD_TIME_BASE / OMXReader::NormalizeFrameduration((double)DVD_TIME_BASE * fpsscale / fpsrate);

  //Supported HDMI CEA/DMT resolutions, preferred resolution will be returned
  TV_SUPPORTED_MODE_NEW_T *supported_modes = NULL;
  // query the number of modes first
  int max_supported_modes = m_BcmHost.vc_tv_hdmi_get_supported_modes_new(group, NULL, 0, &prefer_group, &prefer_mode);
 
  if (max_supported_modes > 0)
    supported_modes = new TV_SUPPORTED_MODE_NEW_T[max_supported_modes];
 
  if (supported_modes)
  {
    num_modes = m_BcmHost.vc_tv_hdmi_get_supported_modes_new(group,
        supported_modes, max_supported_modes, &prefer_group, &prefer_mode);

    CLog::Log(LOGDEBUG, "EGL get supported modes (%d) = %d, prefer_group=%x, prefer_mode=%x\n",
        group, num_modes, prefer_group, prefer_mode);
  }

  TV_SUPPORTED_MODE_NEW_T *tv_found = NULL;

  if (num_modes > 0 && prefer_group != HDMI_RES_GROUP_INVALID)
  {
    uint32_t best_score = 1<<30;
    uint32_t scan_mode = 0;

    for (i=0; i<num_modes; i++)
    {
      TV_SUPPORTED_MODE_NEW_T *tv = supported_modes + i;
      uint32_t score = 0;
      uint32_t w = tv->width;
      uint32_t h = tv->height;
      uint32_t r = tv->frame_rate;

      /* Check if frame rate match (equal or exact multiple) */
      if(fabs(r - 1.0f*fps) / fps < 0.002f)
	score += 0;
      else if(fabs(r - 2.0f*fps) / fps < 0.002f)
	score += 1<<8;
      else 
	score += (1<<28)/r; // bad - but prefer higher framerate

      /* Check size too, only choose, bigger resolutions */
      if(width && height) 
      {
        /* cost of too small a resolution is high */
        score += max((int)(width -w), 0) * (1<<16);
        score += max((int)(height-h), 0) * (1<<16);
        /* cost of too high a resolution is lower */
        score += max((int)(w-width ), 0) * (1<<4);
        score += max((int)(h-height), 0) * (1<<4);
      } 

      // native is good
      if (!tv->native) 
        score += 1<<16;

      // interlace is bad
      if (scan_mode != tv->scan_mode) 
        score += (1<<16);

      // wanting 3D but not getting it is a negative
      if (is3d == CONF_FLAGS_FORMAT_SBS && !(tv->struct_3d_mask & HDMI_3D_STRUCT_SIDE_BY_SIDE_HALF_HORIZONTAL))
        score += 1<<18;
      if (is3d == CONF_FLAGS_FORMAT_TB  && !(tv->struct_3d_mask & HDMI_3D_STRUCT_TOP_AND_BOTTOM))
        score += 1<<18;

      // prefer square pixels modes
      float par = get_display_aspect_ratio((HDMI_ASPECT_T)tv->aspect_ratio)*(float)tv->height/(float)tv->width;
      score += fabs(par - 1.0f) * (1<<12);

      /*printf("mode %dx%d@%d %s%s:%x par=%.2f score=%d\n", tv->width, tv->height, 
             tv->frame_rate, tv->native?"N":"", tv->scan_mode?"I":"", tv->code, par, score);*/

      if (score < best_score) 
      {
        tv_found = tv;
        best_score = score;
      }
    }
  }

  if(tv_found)
  {
    if (! dump_mode)
      printf("Output mode %d: %dx%d@%d %s%s:%x\n", tv_found->code, tv_found->width, tv_found->height, 
	     tv_found->frame_rate, tv_found->native?"N":"", tv_found->scan_mode?"I":"", tv_found->code);
    // if we are closer to ntsc version of framerate, let gpu know
    int ifps = (int)(fps+0.5f);
    bool ntsc_freq = fabs(fps*1001.0f/1000.0f - ifps) < fabs(fps-ifps);
    char response[80];
    vc_gencmd(response, sizeof response, "hdmi_ntsc_freqs %d", ntsc_freq);

    /* inform TV of any 3D settings. Note this property just applies to next hdmi mode change, so no need to call for 2D modes */
    HDMI_PROPERTY_PARAM_T property;
    property.property = HDMI_PROPERTY_3D_STRUCTURE;
    property.param1 = HDMI_3D_FORMAT_NONE;
    property.param2 = 0;
    if (is3d != CONF_FLAGS_FORMAT_NONE)
    {
      if (is3d == CONF_FLAGS_FORMAT_SBS && tv_found->struct_3d_mask & HDMI_3D_STRUCT_SIDE_BY_SIDE_HALF_HORIZONTAL)
        property.param1 = HDMI_3D_FORMAT_SBS_HALF;
      else if (is3d == CONF_FLAGS_FORMAT_TB && tv_found->struct_3d_mask & HDMI_3D_STRUCT_TOP_AND_BOTTOM)
        property.param1 = HDMI_3D_FORMAT_TB_HALF;
      m_BcmHost.vc_tv_hdmi_set_property(&property);
    }

    if (! dump_mode)
      printf("ntsc_freq:%d %s%s\n", ntsc_freq, property.param1 == HDMI_3D_FORMAT_SBS_HALF ? "3DSBS":"", property.param1 == HDMI_3D_FORMAT_TB_HALF ? "3DTB":"");
    m_BcmHost.vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_HDMI, (HDMI_RES_GROUP_T)group, tv_found->code);
  }
}

bool SyncVideo::Exists(const std::string& path)
{
  struct stat buf;
  auto error = stat(path.c_str(), &buf);
  return !error || errno != ENOENT;
}

bool SyncVideo::IsURL(const std::string& str)
{
  auto result = str.find("://");
  if(result == std::string::npos || result == 0)
    return false;

  for(size_t i = 0; i < result; ++i)
  {
    if(!isalpha(str[i]))
      return false;
  }
  return true;
}

bool SyncVideo::omxplayer_remap_wanted(void)
{
  bool remap = m_remap;
  m_remap = false;
  return remap;
}
















int SyncVideo::play()
{
// signal(SIGINT, sig_handler);
bool m_first = true;
#if WANT_KEYS
  if (isatty(STDIN_FILENO))
  {
    struct termios new_termios;

    tcgetattr(STDIN_FILENO, &orig_termios);

    new_termios             = orig_termios;
    new_termios.c_lflag     &= ~(ICANON | ECHO | ECHOCTL | ECHONL);
    new_termios.c_cflag     |= HUPCL;
    new_termios.c_cc[VMIN]  = 0;


    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    atexit(restore_termios);
  }
  else
  {
    orig_fl = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, orig_fl | O_NONBLOCK);
    atexit(restore_fl);
  }
#endif /* WANT_KEYS */

  
  double                m_incr                = 0;
  CRBP                  g_RBP;
  COMXCore              g_OMX;
  bool                  m_quiet               = false;
  bool                  m_stats               = false;
  bool                  m_dump_format         = false;
  FORMAT_3D_T           m_3d                  = CONF_FLAGS_FORMAT_NONE;
  bool                  m_refresh             = false;
  double                startpts              = 0;
  CRect                 DestRect              = {0,0,0,0};
  TV_DISPLAY_STATE_T   tv_state;
  GError                *error = NULL;

  const int font_opt      = 0x100;
  const int font_size_opt = 0x101;
  const int align_opt     = 0x102;
  const int subtitles_opt = 0x103;
  const int lines_opt     = 0x104;
  const int pos_opt       = 0x105;
  const int vol_opt       = 0x106;
  const int boost_on_downmix_opt = 0x200;
  const int tile_code_opt = 0x300;
  const int frame_size_opt = 0x301;

  //LOOP
  double loop_offset     = 0.0;
  double last_packet_pts = 0.0;
  double last_packet_dts = 0.0;
  double last_packet_duration = 0.0;

  
 //  int c;
 //  std::string mode;
 //  while ((c = getopt_long(argc, argv, "wihn:l:o:cqslpd3:yzt:rW:T:O:F:AR:C:", longopts, NULL)) != -1)
 //  {
 //    switch (c) 
 //    {
 //      case 'q':
 //        m_quiet = true;
 //        break;
    
 //      case 'l':
 //        m_seek_pos = atoi(optarg) ;
 //        if (m_seek_pos < 0)
 //            m_seek_pos = 0;
 //        break;
 
 //      case pos_opt:
	// sscanf(optarg, "%f %f %f %f", &DestRect.x1, &DestRect.y1, &DestRect.x2, &DestRect.y2);
 //        break;
 
 //      case tile_code_opt:
	// if (! m_tilemap) m_tilemap = pwtilemap_create();
	// pwtilemap_set_tilecode(m_tilemap, atoi(optarg));
	// break;
 //      case frame_size_opt:
	// {
	//   float fx, fy;
	//   char c;
	//   if (! m_tilemap) m_tilemap = pwtilemap_create();
	//   if (sscanf(optarg, "%gx%g%c", &fx, &fy, &c) != 2) {
	//     fprintf(stderr, "pwomxplayer --frame-size: invalid FXxFY\n");
	//     return 2;
	//   }
	//   pwtilemap_set_framesize(m_tilemap, fx, fy);
	// }
	// break;
 //      case 'W':
	// {
	//   PwRect wall;
	//   if (! m_tilemap) m_tilemap = pwtilemap_create();
	//   if (! pwrect_from_string(&wall, optarg, &error)) {
	//     fprintf(stderr, "pwomxplayer --wall: %s\n", error->message);
	//     return 2;
	//   } else {
	//     pwtilemap_set_wall(m_tilemap, &wall);
	//   }
	// }
	// break;
 //      case 'T':
	// {
	//   PwRect tile;
	//   if (! m_tilemap) m_tilemap = pwtilemap_create();
	//   if (! pwrect_from_string(&tile, optarg, &error)) {
	//     fprintf(stderr, "pwomxplayer --tile: %s\n", error->message);
	//     return 2;
	//   } else {
	//     pwtilemap_set_tile(m_tilemap, &tile);
	//   }
	// }
	// break;
 //      case 'O':
	// {
	//   PwOrient orient;
	//   if (! m_tilemap) m_tilemap = pwtilemap_create();
	//   if (! pworient_from_string(&orient, optarg, &error)) {
	//     fprintf(stderr, "pwomxplayer --orient: %s\n", error->message);
	//     return 2;
	//   } else {
	//     pwtilemap_set_orient(m_tilemap, orient);
	//   }
	// }
	// break;
 //      case 'F':
	// {
	//   PwFit fit;
	//   if (! m_tilemap) m_tilemap = pwtilemap_create();
	//   if (! pwfit_from_string(&fit, optarg, &error)) {
	//     fprintf(stderr, "pwomxplayer --fit: %s\n", error->message);
	//     return 2;
	//   } else {
	//     pwtilemap_set_fit(m_tilemap, fit);
	//   }
	// }
	// break;
 //      case 'A':
	// if (! m_tilemap) m_tilemap = pwtilemap_create();
	// pwtilemap_set_auto(m_tilemap);
	// break;
 //      case 'R':
	// if (! m_tilemap) m_tilemap = pwtilemap_create();
	// pwtilemap_set_role(m_tilemap, optarg);
	// break;
 //      case 'C':
	// if (! m_tilemap) m_tilemap = pwtilemap_create();
	// pwtilemap_set_config(m_tilemap, optarg);
	// break;
 //      case 0:
 //        break;
 //      case 'h':
 //        print_usage();
 //        return 0;
 //        break;
 //      case ':':
 //        return 0;
 //        break;
 //      default:
 //        return 0;
 //        break;
 //    }
 //  }

  

  


//changement de taille ICI
  if(m_wallWidth > 0 && m_wallHeight > 0)
  {
    ostringstream  wallStr;
    wallStr << m_wallWidth << "x" << m_wallHeight << "+0+0";
    cout << "WALL " << wallStr.str() << endl;
    PwRect wall;
    if (! m_tilemap) m_tilemap = pwtilemap_create();
    if (! pwrect_from_string(&wall, wallStr.str().c_str(), &error)) {
      fprintf(stderr, "pwomxplayer --wall: %s\n", error->message);
      return 2;
    } else {
      pwtilemap_set_wall(m_tilemap, &wall);
    }
  }

  if(m_tileWidth > 0 && m_tileHeight > 0 && m_tileX >= 0 && m_tileY >= 0)
  {
    ostringstream tileStr;
    tileStr << m_tileWidth << "x" << m_tileHeight << "+" << m_tileX  << "+" << m_tileY;
    cout << "TILE " << tileStr.str() << endl;
    PwRect tile;  
    if (! m_tilemap) m_tilemap = pwtilemap_create();
    if (! pwrect_from_string(&tile, tileStr.str().c_str(), &error)) {
      fprintf(stderr, "pwomxplayer --tile: %s\n", error->message);
      return 2;
    } else {
      pwtilemap_set_tile(m_tilemap, &tile);
    }
  }


  auto PrintFileNotFound = [](const std::string& path)
  {
    printf("File \"%s\" not found.\n", path.c_str());
  };

  bool filename_is_URL = IsURL(m_filename);

  if(!filename_is_URL && !Exists(m_filename))
  {
    PrintFileNotFound(m_filename);
    return 0;
  }

  if(m_has_font && !Exists(m_font_path))
  {
    PrintFileNotFound(m_font_path);
    return 0;
  }

  if(m_has_external_subtitles && !Exists(m_external_subtitles_path))
  {
    PrintFileNotFound(m_external_subtitles_path);
    return 0;
  }

  if(!m_has_external_subtitles && !filename_is_URL)
  {
    auto subtitles_path = m_filename.substr(0, m_filename.find_last_of(".")) +
                          ".srt";

    if(Exists(subtitles_path))
    {
      m_external_subtitles_path = subtitles_path;
      m_has_external_subtitles = true;
    }
  }

  if (m_tilemap != NULL) {
    if (! pwtilemap_define(m_tilemap, &error)) {
      fprintf(stderr, "pwomxplayer: %s\n", error->message);
      return 1;
    }
  }


  g_RBP.Initialize();
  g_OMX.Initialize();

  m_av_clock = new OMXClock();

  m_thread_player = true;

  if(!m_omx_reader.Open(m_filename.c_str(), m_dump_format, ! m_quiet))
    goto do_exit;

  if(m_dump_format)
    goto do_exit;

  //LOOP
  if(m_loop && !m_omx_reader.CanSeek()) {
    printf("Looping requested on an input that doesn't support seeking\n");
    goto do_exit;
  }

  struct timeval now;
  gettimeofday(&now, NULL);

  //start date is over and we can navigate through the stream
  if(m_dateStart < now.tv_sec && m_omx_reader.CanSeek())
  {

    if(m_omx_reader.GetStreamLength() < 15000)
    {
      
      struct tm * timeinfo;
      char buffer [80];

      timeinfo = localtime (&(m_dateStart));
      strftime (buffer,80," %I:%M:%S%p.",timeinfo);
      printf("previous time %s\n", buffer);

      m_seek_pos = 0;
      m_dateStart = now.tv_sec + 15 + m_omx_reader.GetStreamLength()/1000 - ((now.tv_sec+15)%(m_omx_reader.GetStreamLength()/1000));
      

      timeinfo = localtime (&(m_dateStart));
      strftime (buffer,80," %I:%M:%S%p.",timeinfo);
      printf("after time %s\n", buffer);


    }else
    {
      //but there is still some video to play (with a safety)
      if(m_dateEnd > now.tv_sec + 15)
      {
        printf("Start on the go\n");
        m_seek_pos = now.tv_sec - m_dateStart + 15;

        if(m_seek_pos > m_omx_reader.GetStreamLength()/1000 && m_loop)
        {
          printf("seekpos %i\n", m_seek_pos);
          printf("duration %i\n", m_omx_reader.GetStreamLength());
          m_seek_pos = m_seek_pos % (m_omx_reader.GetStreamLength()/1000);
          printf("new seekpos %i\n", m_seek_pos);
        }

        m_dateStart = now.tv_sec + 15;
      }
    }
  }


  m_bMpeg         = m_omx_reader.IsMpegVideo();
  m_has_video     = m_omx_reader.VideoStreamCount();
  m_has_audio     = m_omx_reader.AudioStreamCount();
  m_has_subtitle  = m_has_external_subtitles ||
                    m_omx_reader.SubtitleStreamCount();

  if(m_filename.find("3DSBS") != string::npos || m_filename.find("HSBS") != string::npos)
    m_3d = CONF_FLAGS_FORMAT_SBS;
  else if(m_filename.find("3DTAB") != string::npos || m_filename.find("HTAB") != string::npos)
    m_3d = CONF_FLAGS_FORMAT_TB;

  // 3d modes don't work without switch hdmi mode
  if (m_3d != CONF_FLAGS_FORMAT_NONE)
    m_refresh = true;

  // you really don't want want to match refresh rate without hdmi clock sync
  if (m_refresh && !m_no_hdmi_clock_sync)
    m_hdmi_clock_sync = true;

  if(!m_av_clock->OMXInitialize(m_has_video, m_has_audio))
    goto do_exit;

  if(m_hdmi_clock_sync && !m_av_clock->HDMIClockSync())
      goto do_exit;

  m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_hints_audio);
  m_omx_reader.GetHints(OMXSTREAM_VIDEO, m_hints_video);

  if(m_audio_index_use != -1)
    m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, m_audio_index_use);
          
  if(m_has_video && m_refresh)
  {
    memset(&tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
    m_BcmHost.vc_tv_get_display_state(&tv_state);

    SetVideoMode(m_hints_video.width, m_hints_video.height, m_hints_video.fpsrate, m_hints_video.fpsscale, m_3d, ! m_quiet);
  }
  // get display aspect
  TV_DISPLAY_STATE_T current_tv_state;
  memset(&current_tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
  m_BcmHost.vc_tv_get_display_state(&current_tv_state);
  if(current_tv_state.state & ( VC_HDMI_HDMI | VC_HDMI_DVI )) {
    //HDMI or DVI on
    m_display_aspect = get_display_aspect_ratio((HDMI_ASPECT_T)current_tv_state.display.hdmi.aspect_ratio);
  } else {
    //composite on
    m_display_aspect = get_display_aspect_ratio((SDTV_ASPECT_T)current_tv_state.display.sdtv.display_options.aspect);
  }
  m_display_aspect *= (float)current_tv_state.display.hdmi.height/(float)current_tv_state.display.hdmi.width;

  if (m_tilemap != NULL) {
    PwIntRect screen;
    PWRECT_SET0(screen,
		current_tv_state.display.hdmi.width,
		current_tv_state.display.hdmi.height);
    pwtilemap_set_screen(m_tilemap, &screen);
  }

  // seek on start
  if (m_seek_pos !=0 && m_omx_reader.CanSeek()) {
    if (! m_quiet)
      printf("Seeking start of video to %i seconds\n", m_seek_pos);
    m_omx_reader.SeekTime(m_seek_pos * 1000.0f, 0, &startpts);  // from seconds to DVD_TIME_BASE
  }
  
  if(m_has_video && !m_player_video.Open(m_hints_video, m_av_clock, m_tilemap, DestRect, ! m_quiet, m_Deinterlace,  m_bMpeg,
                                         m_hdmi_clock_sync, m_thread_player, m_display_aspect))
    goto do_exit;

  {
    std::vector<Subtitle> external_subtitles;
    if(m_has_external_subtitles &&
       !ReadSrt(m_external_subtitles_path, external_subtitles))
    {
       puts("Unable to read the subtitle file.");
       goto do_exit;
    }

    if(m_has_subtitle &&
       !m_player_subtitles.Open(m_omx_reader.SubtitleStreamCount(),
                                std::move(external_subtitles),
                                m_font_path,
                                m_font_size,
                                m_centered,
                                m_subtitle_lines,
                                m_av_clock))
      goto do_exit;
  }

  if(m_has_subtitle)
  {
    if(!m_has_external_subtitles)
    {
      if(m_subtitle_index != -1)
      {
        m_player_subtitles.SetActiveStream(
          std::min(m_subtitle_index, m_omx_reader.SubtitleStreamCount()-1));
      }
      m_player_subtitles.SetUseExternalSubtitles(false);
    }

    if(m_subtitle_index == -1 && !m_has_external_subtitles)
      m_player_subtitles.SetVisible(false);
  }

  m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_hints_audio);

  if (deviceString == "")
  {
    if (m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_ePCM, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) == 0)
      deviceString = "omx:hdmi";
    else
      deviceString = "omx:local";
  }

  if(m_has_audio && !m_player_audio.Open(m_hints_audio, m_av_clock, &m_omx_reader, deviceString, 
                                         m_passthrough, m_initialVolume, m_use_hw_audio,
                                         m_boost_on_downmix, m_thread_player))
    goto do_exit;

   


  m_av_clock->SetSpeed(DVD_PLAYSPEED_NORMAL);
  m_av_clock->OMXStateExecute();
  m_av_clock->OMXStart(0.0);

  struct timespec starttime, endtime;

  if (! m_quiet)
    PrintSubtitleInfo();



  while(!m_stop)
  {
    //verifie le temps de fin
    struct timeval now;
    gettimeofday(&now, NULL);
    if(now.tv_sec > m_dateEnd)
    {
      Stop();
      goto do_exit;
    }



    if(SyncVideo::g_abort)
      goto do_exit;

// #if WANT_KEYS
//     int ch[8];
//     int chnum = 0;
// #endif

    
// #if WANT_KEYS
//     while((ch[chnum] = getchar()) != EOF) chnum++;
//     if (chnum > 1) ch[0] = ch[chnum - 1] | (ch[chnum - 2] << 8);

//     switch(ch[0])
//     {
//       case 'z':
//         m_tv_show_info = !m_tv_show_info;
//         vc_tv_show_info(m_tv_show_info);
//         break;
//       case '1':
//         SetSpeed(m_av_clock->OMXPlaySpeed() - 1);
//         break;
//       case '2':
//         SetSpeed(m_av_clock->OMXPlaySpeed() + 1);
//         break;
//       case 'j':
//         if(m_has_audio)
//         {
//           int new_index = m_omx_reader.GetAudioIndex() - 1;
//           if (new_index >= 0)
//             m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, new_index);
//         }
//         break;
//       case 'k':
//         if(m_has_audio)
//           m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, m_omx_reader.GetAudioIndex() + 1);
//         break;
//       case 'i':
//         if(m_omx_reader.GetChapterCount() > 0)
//         {
//           m_omx_reader.SeekChapter(m_omx_reader.GetChapter() - 1, &startpts);
//           FlushStreams(startpts);
//         }
//         else
//         {
//           m_incr = -600.0;
//         }
//         break;
//       case 'o':
//         if(m_omx_reader.GetChapterCount() > 0)
//         {
//           m_omx_reader.SeekChapter(m_omx_reader.GetChapter() + 1, &startpts);
//           FlushStreams(startpts);
//         }
//         else
//         {
//           m_incr = 600.0;
//         }
//         break;
//       case 'n':
//         if(m_has_subtitle)
//         {
//           if(!m_player_subtitles.GetUseExternalSubtitles())
//           {
//             if (m_player_subtitles.GetActiveStream() == 0)
//             {
//               if(m_has_external_subtitles)
//                 m_player_subtitles.SetUseExternalSubtitles(true);
//             }
//             else
//             {
//               m_player_subtitles.SetActiveStream(
//                 m_player_subtitles.GetActiveStream()-1);
//             }
//           }

//           m_player_subtitles.SetVisible(true);
//           PrintSubtitleInfo();
//         }
//         break;
//       case 'm':
//         if(m_has_subtitle)
//         {
//           if(m_player_subtitles.GetUseExternalSubtitles())
//           {
//             if(m_omx_reader.SubtitleStreamCount())
//             {
//               assert(m_player_subtitles.GetActiveStream() == 0);
//               m_player_subtitles.SetUseExternalSubtitles(false);
//             }
//           }
//           else
//           {
//             auto new_index = m_player_subtitles.GetActiveStream()+1;
//             if(new_index < (size_t) m_omx_reader.SubtitleStreamCount())
//               m_player_subtitles.SetActiveStream(new_index);
//           }

//           m_player_subtitles.SetVisible(true);
//           PrintSubtitleInfo();
//         }
//         break;
//       case 's':
//         if(m_has_subtitle)
//         {
//           m_player_subtitles.SetVisible(!m_player_subtitles.GetVisible());
//           PrintSubtitleInfo();
//         }
//         break;
//       case 'd':
//         if(m_has_subtitle && m_player_subtitles.GetVisible())
//         {
//           m_player_subtitles.SetDelay(m_player_subtitles.GetDelay() - 250);
//           PrintSubtitleInfo();
//         }
//         break;
//       case 'f':
//         if(m_has_subtitle && m_player_subtitles.GetVisible())
//         {
//           m_player_subtitles.SetDelay(m_player_subtitles.GetDelay() + 250);
//           PrintSubtitleInfo();
//         }
//         break;
//       case 'q': case 27:
//         m_stop = true;
//         goto do_exit;
//         break;
//       case 0x5b44: // key left
//         if(m_omx_reader.CanSeek()) m_incr = -30.0;
//         break;
//       case 0x5b43: // key right
//         if(m_omx_reader.CanSeek()) m_incr = 30.0;
//         break;
//       case 0x5b41: // key up
//         if(m_omx_reader.CanSeek()) m_incr = 600.0;
//         break;
//       case 0x5b42: // key down
//         if(m_omx_reader.CanSeek()) m_incr = -600.0;
//         break;
//       case ' ':
//       case 'p':
//         m_Pause = !m_Pause;
//         if(m_Pause)
//         {
//           SetSpeed(OMX_PLAYSPEED_PAUSE);
//           m_av_clock->OMXPause();
//           if(m_has_subtitle)
//             m_player_subtitles.Pause();
//         }
//         else
//         {
//           if(m_has_subtitle)
//             m_player_subtitles.Resume();
//           SetSpeed(OMX_PLAYSPEED_NORMAL);
//           m_av_clock->OMXResume();
//         }
//         break;
//       case '-':
//         m_player_audio.SetCurrentVolume(m_player_audio.GetCurrentVolume() - 300);
//         if (! m_quiet)
// 	  printf("Current Volume: %.2fdB\n", m_player_audio.GetCurrentVolume() / 100.0f);
//         break;
//       case '+': case '=':
//         m_player_audio.SetCurrentVolume(m_player_audio.GetCurrentVolume() + 300);
//         if (! m_quiet)
// 	  printf("Current Volume: %.2fdB\n", m_player_audio.GetCurrentVolume() / 100.0f);
//         break;
//       default:
//         break;
//     }
// #endif /* WANT_KEYS */

    if(m_Pause)
    {
      OMXClock::OMXSleep(2);
      continue;
    }

    if(m_incr != 0 && !m_bMpeg)
    {
      int    seek_flags   = 0;
      double seek_pos     = 0;
      double pts          = 0;

      if(m_has_subtitle)
        m_player_subtitles.Pause();

      m_av_clock->OMXStop();

      pts = m_av_clock->GetPTS();

      seek_pos = (pts / DVD_TIME_BASE) + m_incr;
      seek_flags = m_incr < 0.0f ? AVSEEK_FLAG_BACKWARD : 0;

      seek_pos *= 1000.0f;

      m_incr = 0;

      if(m_omx_reader.SeekTime(seek_pos, seek_flags, &startpts))
        FlushStreams(startpts);

      m_player_video.Close();
      if(m_has_video && !m_player_video.Open(m_hints_video, m_av_clock, m_tilemap, DestRect, ! m_quiet, m_Deinterlace, m_bMpeg,
                                         m_hdmi_clock_sync, m_thread_player, m_display_aspect))
        goto do_exit;

      m_av_clock->OMXStart(startpts);
      
      if(m_has_subtitle)
        m_player_subtitles.Resume();
    }

    /* player got in an error state */
    if(m_player_audio.Error())
    {
      fprintf(stderr, "audio player error. emergency exit!!!\n");
      goto do_exit;
    }

    if(m_stats)
    {
      printf("V : %8.02f %8d %8d A : %8.02f %8.02f Cv : %8d Ca : %8d                            \r",
             m_av_clock->OMXMediaTime(), m_player_video.GetDecoderBufferSize(),
             m_player_video.GetDecoderFreeSpace(), m_player_audio.GetCurrentPTS() / DVD_TIME_BASE, 
             m_player_audio.GetDelay(), m_player_video.GetCached(), m_player_audio.GetCached());
    }



    //DEBUT LOOP
    if(m_loop && m_omx_reader.IsEof() && !m_omx_pkt && m_isOver) {
      m_isOver = false;
      CLog::Log(LOGINFO, "EOF detected; looping requested");
      printf("RESTART !!\n");
      m_incr = -DBL_MAX;
      m_first= true;
      
      struct timeval now;
      gettimeofday(&now, NULL); 
      m_dateStart += (m_omx_reader.GetStreamLength()/1000) + 2 - m_seek_pos;
      printf("length of stream %i\n", m_omx_reader.GetStreamLength()/1000);
      printf("seek %i\n", m_seek_pos);
      m_seek_pos = 0;
    }







    if(m_omx_reader.IsEof() && !m_omx_pkt)
    {
      if (!m_player_audio.GetCached() && !m_player_video.GetCached())
      {
        if(m_loop)
        {
          cout << "abort ABORT !!" << endl;
          m_isOver = true;
          if(m_has_audio)
            m_player_audio.WaitCompletion();
          else if(m_has_video)
            m_player_video.WaitCompletion();


        }else
        {
          break;
        }
      }

      // Abort audio buffering, now we're on our own
      if (m_buffer_empty && m_first)
      {
        printf("en attente (empty buffer)\n");
        m_timer.SleepUntilTime(m_dateStart);
        printf("GO\n");
        m_av_clock->OMXResume();
        m_first = false;
      }

      OMXClock::OMXSleep(10);
      continue;
    }

    /* when the audio buffer runs under 0.1 seconds we buffer up */
    if(m_has_audio)
    {
      if(m_player_audio.GetDelay() < 0.1f && !m_buffer_empty)
      {
        if(!m_av_clock->OMXIsPaused())
        {
          m_av_clock->OMXPause();
          //printf("buffering start\n");
          m_buffer_empty = true;
          clock_gettime(CLOCK_REALTIME, &starttime);
        }
      }
      // printf("A\n");
      if(m_player_audio.GetDelay() > (AUDIO_BUFFER_SECONDS * 0.75f) && m_buffer_empty)
      {
        if(m_av_clock->OMXIsPaused())
        {
          printf("en attente\n");
          m_timer.SleepUntilTime(m_dateStart);
          m_av_clock->OMXResume();
          printf("GO\n");
          m_buffer_empty = false;
        }
      }
      if(m_buffer_empty)
      {
        clock_gettime(CLOCK_REALTIME, &endtime);
        if((endtime.tv_sec - starttime.tv_sec) > 1)
        {
          m_buffer_empty = false;
          m_av_clock->OMXResume();
          //printf("buffering timed out\n");
        }
      }
    }

    if(!m_omx_pkt)
      m_omx_pkt = m_omx_reader.Read();

    if(m_has_video && m_omx_pkt && m_omx_reader.IsActive(OMXSTREAM_VIDEO, m_omx_pkt->stream_index))
    {
      if(m_player_video.AddPacket(m_omx_pkt))
        m_omx_pkt = NULL;
      else
        OMXClock::OMXSleep(10);

      if(m_tv_show_info)
      {
        char response[80];
        vc_gencmd(response, sizeof response, "render_bar 4 video_fifo %d %d %d %d", 
                m_player_video.GetDecoderBufferSize()-m_player_video.GetDecoderFreeSpace(),
                0 , 0, m_player_video.GetDecoderBufferSize());
        vc_gencmd(response, sizeof response, "render_bar 5 audio_fifo %d %d %d %d", 
                (int)(100.0*m_player_audio.GetDelay()), 0, 0, 100*AUDIO_BUFFER_SECONDS);
      }
    }
    else if(m_has_audio && m_omx_pkt && m_omx_pkt->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if(m_player_audio.AddPacket(m_omx_pkt))
        m_omx_pkt = NULL;
      else
        OMXClock::OMXSleep(10);
    }
    else if(m_has_subtitle && m_omx_pkt &&
            m_omx_pkt->codec_type == AVMEDIA_TYPE_SUBTITLE)
    {
      auto result = m_player_subtitles.AddPacket(m_omx_pkt,
                      m_omx_reader.GetRelativeIndex(m_omx_pkt->stream_index));
      if (result)
        m_omx_pkt = NULL;
      else
        OMXClock::OMXSleep(10);
    }
    else
    {
      if(m_omx_pkt)
      {
        m_omx_reader.FreePacket(m_omx_pkt);
        m_omx_pkt = NULL;
      }
    }
  }

do_exit:
  if (m_stats)
    printf("\n");

  if(!m_stop && !SyncVideo::g_abort)
  {
    cout << "WAIT" << endl;
    if(m_has_audio)
      m_player_audio.WaitCompletion();
    else if(m_has_video)
      m_player_video.WaitCompletion();

    cout << "WAIT OVER" << endl;
  }

  if(m_has_video && m_refresh && tv_state.display.hdmi.group && tv_state.display.hdmi.mode)
  {
    m_BcmHost.vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_HDMI, (HDMI_RES_GROUP_T)tv_state.display.hdmi.group, tv_state.display.hdmi.mode);
  }

  m_av_clock->OMXStop();
  m_av_clock->OMXStateIdle();

  m_player_subtitles.Close();
  m_player_video.Close();
  m_player_audio.Close();

  if(m_omx_pkt)
  {
    m_omx_reader.FreePacket(m_omx_pkt);
    m_omx_pkt = NULL;
  }

  m_omx_reader.Close();

  vc_tv_show_info(0);

  g_OMX.Deinitialize();
  g_RBP.Deinitialize();

  if (! m_quiet)
    printf("have a nice day ;)\n");
  return 1;
}

