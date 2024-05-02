#include "gamemusic.h"
#include "gothic.h"

#include <Tempest/Sound>
#include <Tempest/Log>

#include "game/definitions/musicdefinitions.h"
#include "resources.h"

#include "dmusic.h"

using namespace Tempest;

struct GameMusic::MusicProducer : Tempest::SoundProducer {
  MusicProducer():SoundProducer(44100,2){
    DmPerformance_create(&mix, 44100);
    }

  void renderSound(int16_t* out,size_t n) override {
    updateTheme();
    DmPerformance_renderPcm(mix, out, n * 2, (DmRenderOptions) (DmRender_SHORT | DmRender_STEREO));
    }

  void updateTheme() {
    zenkit::IMusicTheme     theme;
    bool                    updateTheme = false;
    bool                    reloadTheme = false;
    Tags                    tags        = Tags::Day;

    {
      std::lock_guard<std::mutex> guard(pendingSync);
      if(hasPending && enable.load()) {
        hasPending  = false;
        updateTheme = true;
        reloadTheme = this->reloadTheme;
        theme       = pendingMusic;
        tags        = pendingTags;
        }
    }

    if(!updateTheme)
      return;
    updateTheme = false;

    try {
      if(reloadTheme) {
        DmSegment* p = Resources::loadDxMusic(theme.file);

        DmEmbellishmentType embellishment = DmEmbellishment_NONE;
        switch(theme.transtype) {
        case zenkit::MusicTransitionEffect::UNKNOWN:
        case zenkit::MusicTransitionEffect::NONE:
            embellishment = DmEmbellishment_NONE;
            break;
        case zenkit::MusicTransitionEffect::GROOVE:
            embellishment = DmEmbellishment_GROOVE;
            break;
        case zenkit::MusicTransitionEffect::FILL:
            embellishment = DmEmbellishment_FILL;
            break;
        case zenkit::MusicTransitionEffect::BREAK:
            embellishment = DmEmbellishment_BREAK;
            break;
        case zenkit::MusicTransitionEffect::INTRO:
            embellishment = DmEmbellishment_INTRO;
            break;
        case zenkit::MusicTransitionEffect::END:
            embellishment = DmEmbellishment_END;
            break;
        case zenkit::MusicTransitionEffect::END_AND_INTO:
            embellishment = DmEmbellishment_END_AND_INTRO;
            break;
        }

        DmTiming timing = DmTiming_MEASURE;
        switch (theme.transsubtype) {
        case zenkit::MusicTransitionType::UNKNOWN:
        case zenkit::MusicTransitionType::MEASURE:
            timing = DmTiming_MEASURE;
            break;
        case zenkit::MusicTransitionType::IMMEDIATE:
            timing = DmTiming_INSTANT;
            break;
        case zenkit::MusicTransitionType::BEAT:
            timing = DmTiming_BEAT;
            break;
        }

        DmPerformance_playTransition(mix, p, embellishment, timing);
        DmSegment_release(p);
        currentTags=tags;
        }
      DmPerformance_setVolume(mix, theme.vol * Gothic::settingsGetF("SOUND","musicVolume"));
      }
    catch(std::runtime_error&) {
      Log::e("unable to load sound: \"",theme.file,"\"");
      stopMusic();
      }
    catch(std::bad_alloc&) {
      Log::e("out of memory for sound: \"",theme.file,"\"");
      stopMusic();
      }
    }

  bool setMusic(const zenkit::IMusicTheme &theme, Tags tags){
    std::lock_guard<std::mutex> guard(pendingSync);
    reloadTheme  = pendingMusic.file!=theme.file;
    pendingMusic = theme;
    pendingTags  = tags;
    hasPending   = true;
    return true;
    }

  void restartMusic(){
    std::lock_guard<std::mutex> guard(pendingSync);
    hasPending  = true;
    reloadTheme = true;
    enable.store(true);
    }

  void stopMusic() {
    enable.store(false);
    std::lock_guard<std::mutex> guard(pendingSync);
    DmPerformance_playSegment(mix, NULL, DmTiming_MEASURE);
    }

  void setVolume(float v) {
    DmPerformance_setVolume(mix, v);
    }

  bool isEnabled() const {
    return enable.load();
    }

  DmPerformance*       mix;

  std::mutex           pendingSync;
  std::atomic_bool     enable{true};
  bool                 hasPending=false;
  bool                 reloadTheme=false;
  zenkit::IMusicTheme  pendingMusic;
  Tags                 pendingTags=Tags::Day;
  Tags                 currentTags=Tags::Day;
  };

struct GameMusic::Impl final {
  Impl() {
    std::unique_ptr<MusicProducer> mix(new MusicProducer());
    dxMixer = mix.get();
    dxMixer->setVolume(0.5f);

    sound = device.load(std::move(mix));
    sound.play();
    }

  void setMusic(const zenkit::IMusicTheme &theme, Tags tags) {
    dxMixer->setMusic(theme,tags);
    }

  void setVolume(float v) {
    dxMixer->setVolume(v);
    }

  void setEnabled(bool e) {
    if(isEnabled()==e)
      return;
    if(e) {
      dxMixer->restartMusic();
      sound.play();
      } else {
      dxMixer->stopMusic();
      }
    }

  bool isEnabled() const {
    return dxMixer->isEnabled();
    }

  Tempest::SoundDevice device;
  Tempest::SoundEffect sound;

  MusicProducer*       dxMixer=nullptr;
  };

GameMusic* GameMusic::instance = nullptr;

GameMusic::GameMusic() {
  instance = this;
  impl.reset(new Impl());
  Gothic::inst().onSettingsChanged.bind(this,&GameMusic::setupSettings);
  setupSettings();
  }

GameMusic::~GameMusic() {
  instance = nullptr;
  Gothic::inst().onSettingsChanged.ubind(this,&GameMusic::setupSettings);
  }

GameMusic& GameMusic::inst() {
  return *instance;
  }

GameMusic::Tags GameMusic::mkTags(GameMusic::Tags daytime, GameMusic::Tags mode) {
  return Tags(daytime|mode);
  }

void GameMusic::setEnabled(bool e) {
  impl->setEnabled(e);
  }

bool GameMusic::isEnabled() const {
  return impl->isEnabled();
  }

void GameMusic::setMusic(GameMusic::Music m) {
  const char* clsTheme="";
  switch(m) {
    case GameMusic::SysLoading:
      clsTheme = "SYS_Loading";
      break;
    }
  if(auto theme = Gothic::musicDef()[clsTheme])
    setMusic(*theme,GameMusic::mkTags(GameMusic::Std,GameMusic::Day));
  }

void GameMusic::setMusic(const zenkit::IMusicTheme& theme, Tags tags) {
  impl->setMusic(theme,tags);
  }

void GameMusic::stopMusic() {
  setEnabled(false);
  }

void GameMusic::setupSettings() {
  const int   musicEnabled = Gothic::settingsGetI("SOUND","musicEnabled");
  const float musicVolume  = Gothic::settingsGetF("SOUND","musicVolume");

  setEnabled(musicEnabled!=0);
  impl->setVolume(musicVolume);
  }
