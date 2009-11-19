/*
Software License :

Copyright (c) 2007-2009, The Open Effects Association Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name The Open Effects Association Ltd, nor the names of its 
      contributors may be used to endorse or promote products derived from this
      software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>

#include <string>
#include <map>

// ofx
#include "ofxImageEffect.h"

// ofx host
#include "ofxhBinary.h"
#include "ofxhPropertySuite.h"
#include "ofxhClip.h"
#include "ofxhParam.h"
#include "ofxhMemory.h"
#include "ofxhImageEffect.h"
#include "ofxhPluginAPICache.h"
#include "ofxhPluginCache.h"
#include "ofxhHost.h"
#include "ofxhImageEffectAPI.h"
#include "ofxhXml.h"

// Disable the "this pointer used in base member initialiser list" warning in Windows
namespace OFX {

  namespace Host {

    namespace ImageEffect {

      /// our global host bobject, set when the cache is created
      OFX::Host::ImageEffect::Host *gImageEffectHost;

      /// ctor
#if defined(WINDOWS) && !defined(__GNUC__)
#pragma warning( disable : 4355 )
#endif
      ImageEffectPlugin::ImageEffectPlugin(PluginCache &pc, PluginBinary *pb, int pi, OfxPlugin *pl)
        : Plugin(pb, pi, pl)
        , _pc(pc)
        , _baseDescriptor(NULL)
        , _madeKnownContexts(false)
        , _pluginHandle(0)
      {
        _baseDescriptor = gImageEffectHost->makeDescriptor(this);
      }

      ImageEffectPlugin::ImageEffectPlugin(PluginCache &pc,
                                           PluginBinary *pb,
                                           int pi,
                                           const std::string &api,
                                           int apiVersion,
                                           const std::string &pluginId,
                                           const std::string &rawId,
                                           int pluginMajorVersion,
                                           int pluginMinorVersion)
        : Plugin(pb, pi, api, apiVersion, pluginId, rawId, pluginMajorVersion, pluginMinorVersion)
        , _pc(pc)
        , _baseDescriptor(NULL) 
        , _madeKnownContexts(false)
        , _pluginHandle(0)
      {        
        _baseDescriptor = gImageEffectHost->makeDescriptor(this);
      }

#if defined(WINDOWS) && !defined(__GNUC__)
#pragma warning( default : 4355 )
#endif

      ImageEffectPlugin::~ImageEffectPlugin()
      {
        for(std::map<std::string, Descriptor *>::iterator it =  _contexts.begin(); it != _contexts.end(); ++it) {
          delete it->second;
        }
        _contexts.clear();
        if(_pluginHandle.get()) {
          OfxPlugin *op = _pluginHandle->getOfxPlugin();
          op->mainEntry(kOfxActionUnload, 0, 0, 0);
        }
      }

      APICache::PluginAPICacheI &ImageEffectPlugin::getApiHandler()
      {
        return _pc;
      }


      /// get the image effect descriptor
      Descriptor &ImageEffectPlugin::getDescriptor() {
        return *_baseDescriptor;
      }

      /// get the image effect descriptor const version
      const Descriptor &ImageEffectPlugin::getDescriptor() const {
        return *_baseDescriptor;
      }

      void ImageEffectPlugin::addContext(const std::string &context, Descriptor *ied)
      {
        _contexts[context] = ied;
        _knownContexts.insert(context);
        _madeKnownContexts = true;
      }

      void ImageEffectPlugin::addContext(const std::string &context)
      {
        _knownContexts.insert(context);
        _madeKnownContexts = true;
      }

      void ImageEffectPlugin::addContextInternal(const std::string &context) const
      {
        _knownContexts.insert(context);
        _madeKnownContexts = true;
      }

      void ImageEffectPlugin::saveXML(std::ostream &os) 
      {
        APICache::propertySetXMLWrite(os, getDescriptor().getProperties(), 6);
      }

      const std::set<std::string> &ImageEffectPlugin::getContexts() const {
        if (_madeKnownContexts) {
          return _knownContexts;
        }
        else {
          const OFX::Host::Property::Set &eProps = getDescriptor().getProperties();
          int size = eProps.getDimension(kOfxImageEffectPropSupportedContexts);
          for (int j=0;j<size;j++) {
            std::string context = eProps.getStringProperty(kOfxImageEffectPropSupportedContexts, j);
            addContextInternal(context);
          }
          return _knownContexts;
        }
      }

      PluginHandle *ImageEffectPlugin::getPluginHandle() 
      {
        if(!_pluginHandle.get()) {
          _pluginHandle.reset(new OFX::Host::PluginHandle(this, _pc.getHost())); 

          OfxPlugin *op = _pluginHandle->getOfxPlugin();

          if (!op) {
            _pluginHandle.reset(0);
            return 0;
          }

          int rval = op->mainEntry(kOfxActionLoad, 0, 0, 0);

          if (rval != kOfxStatOK && rval != kOfxStatReplyDefault) {
            _pluginHandle.reset(0);
            return 0;
          }

          rval = op->mainEntry(kOfxActionDescribe, getDescriptor().getHandle(), 0, 0);

          if (rval != kOfxStatOK && rval != kOfxStatReplyDefault) {
            _pluginHandle.reset(0);
            return 0;
          }
        }

        return _pluginHandle.get();
      }

      Descriptor *ImageEffectPlugin::getContext(const std::string &context) 
      {
        std::map<std::string, Descriptor *>::iterator it = _contexts.find(context);

        COUT( "context : " << context );
        if (it != _contexts.end())
        {
          COUT( "found context description : " << it->second->getLabel() );
          return it->second;
        }

        COUT( "ImageEffectPlugin::getContext -- _contexts" );
        for( std::map<std::string, Descriptor *>::const_iterator a = _contexts.begin(); a!=_contexts.end(); ++a )
        {
            COUT( a->second->getLabel() );
        }
        COUT( "ImageEffectPlugin::getContext -- _knownContexts" );
        for( std::set<std::string>::const_iterator a = _knownContexts.begin(); a!=_knownContexts.end(); ++a )
        {
            COUT( *a );
        }

        if (_knownContexts.find(context) == _knownContexts.end())
        {
            COUT_INFOS;
            return 0;
        }

        //        printf("doing context description.\n");

        OFX::Host::Property::PropSpec inargspec[] = {
          { kOfxImageEffectPropContext, OFX::Host::Property::eString, 1, true, context.c_str() },
          { 0 }
        };
        
        OFX::Host::Property::Set inarg(inargspec);

        PluginHandle *ph = getPluginHandle();
        std::auto_ptr<ImageEffect::Descriptor> newContext( gImageEffectHost->makeDescriptor(getDescriptor(), this));
        int rval = kOfxStatFailed;
        if (ph->getOfxPlugin())
            rval = ph->getOfxPlugin()->mainEntry(kOfxImageEffectActionDescribeInContext, newContext->getHandle(), inarg.getHandle(), 0);

        if (rval == kOfxStatOK || rval == kOfxStatReplyDefault)
        {
            COUT_INFOS;
          _contexts[context] = newContext.release();
          return _contexts[context];
        }
        COUT_INFOS;

        return 0;
      }

      ImageEffect::Instance* ImageEffectPlugin::createInstance(const std::string &context, void *clientData)
      {          


        /// todo - we need to make sure action:load is called, then action:describe again
        /// (not because we are expecting the results to change, but because plugin
        /// might get confused otherwise), then a describe_in_context
        getPluginHandle();
        if(_pluginHandle.get()) {
            Descriptor *desc = getContext(context);
            if (!desc)
            {
                COUT( "The plugin doesn't support the context " << context << "." );
                return 0;
            }

            ImageEffect::Instance *instance = gImageEffectHost->newInstance(clientData,
                                                                            this,
                                                                            *desc,
                                                                            context);
            if (instance)
                instance->populate();

            return instance;
        }

        return NULL;
      }

      void ImageEffectPlugin::unload() {
        if (_pluginHandle.get()) {
          (*_pluginHandle)->mainEntry(kOfxActionUnload, 0, 0, 0);
        }
      }

      PluginCache::PluginCache(OFX::Host::ImageEffect::Host &host) 
        : PluginAPICacheI(kOfxImageEffectPluginApi, 1, 1)
        , _currentPlugin(0)
        , _currentProp(0)
        , _currentContext(0)
        , _currentParam(0)
        , _currentClip(0)
        , _host(&host)
      {
        gImageEffectHost = &host;
      }

      PluginCache::~PluginCache() {}       

      /// get the plugin by id.  vermaj and vermin can be specified.  if they are not it will
      /// pick the highest found version.
      ImageEffectPlugin *PluginCache::getPluginById(const std::string &id, int vermaj, int vermin)
      {
        // return the highest version one, which fits the pattern provided
        ImageEffectPlugin *sofar = 0;

        for (std::vector<ImageEffectPlugin *>::iterator i=_plugins.begin();i!=_plugins.end();i++) {
          ImageEffectPlugin *p = *i;

          if (p->getIdentifier() != id) {
            continue;
          }

          if (vermaj != -1 && p->getVersionMajor() != vermaj) {
            continue;
          }

          if (vermin != -1 && p->getVersionMinor() != vermin) {
            continue;
          }

          if (!sofar || p->trumps(sofar)) {
            sofar = p;
          }
        }

        return sofar;
      }

      /// whether we support this plugin.  
      bool PluginCache::pluginSupported(OFX::Host::Plugin *p, std::string &reason) const {
        return gImageEffectHost->pluginSupported(dynamic_cast<OFX::Host::ImageEffect::ImageEffectPlugin *>(p), reason);
      }

      /// get the plugin by label.  vermaj and vermin can be specified.  if they are not it will
      /// pick the highest found version.
      ImageEffectPlugin *PluginCache::getPluginByLabel(const std::string &label, int vermaj, int vermin)
      {
        // return the highest version one, which fits the pattern provided
        ImageEffectPlugin *sofar = 0;

        for (std::vector<ImageEffectPlugin *>::iterator i=_plugins.begin();i!=_plugins.end();i++) {
          ImageEffectPlugin *p = *i;

          if (p->getDescriptor().getProperties().getStringProperty(kOfxPropLabel) != label) {
            continue;
          }

          if (vermaj != -1 && p->getVersionMajor() != vermaj) {
            continue;
          }

          if (vermin != -1 && p->getVersionMinor() != vermin) {
            continue;
          }

          if (!sofar || p->trumps(sofar)) {
            sofar = p;
          }
        }

        return sofar;
      }

      const std::vector<ImageEffectPlugin *>& PluginCache::getPlugins() const
      {
        return _plugins;
      }

      const std::map<std::string, ImageEffectPlugin *>& PluginCache::getPluginsByID() const
      {
        return _pluginsByID;
      }

      /// handle the case where the info needs filling in from the file.  runs the "describe" action on the plugin.
      void PluginCache::loadFromPlugin(Plugin *op) const {
        std::string msg = "loading ";
        msg += op->getRawIdentifier();

        _host->loadingStatus(msg);

        ImageEffectPlugin *p = dynamic_cast<ImageEffectPlugin*>(op);
        assert(p);

        PluginHandle plug(p, _host);

        int rval = plug->mainEntry(kOfxActionLoad, 0, 0, 0);

        if (rval != 0 && rval != 14) {
          std::cerr << "load failed on plugin " << op->getIdentifier() << std::endl;          
          return;
        }

        rval = plug->mainEntry(kOfxActionDescribe, p->getDescriptor().getHandle(), 0, 0);

        if (rval != 0 && rval != 14) {
          std::cerr << "describe failed on plugin " << op->getIdentifier() << std::endl;          
          return;
        }

        const ImageEffect::Descriptor &e = p->getDescriptor();
        const Property::Set &eProps = e.getProperties();

        int size = eProps.getDimension(kOfxImageEffectPropSupportedContexts);

        for (int j=0;j<size;j++) {
          std::string context = eProps.getStringProperty(kOfxImageEffectPropSupportedContexts, j);
          p->addContext(context);
        }

        rval = plug->mainEntry(kOfxActionUnload, 0, 0, 0);
      }      

      /// handler for preparing to read in a chunk of XML from the cache, set up context to do this
      void PluginCache::beginXmlParsing(Plugin *p) {
        _currentPlugin = dynamic_cast<ImageEffectPlugin*>(p);
      }

      /// XML handler : element begins (everything is stored in elements and attributes)       
      void PluginCache::xmlElementBegin(const std::string &el, std::map<std::string, std::string> map) 
      {
        if (el == "apiproperties") {
          return;
        }

        if (el == "context") {
          _currentContext = gImageEffectHost->makeDescriptor(_currentPlugin->getBinary()->getBundlePath(), _currentPlugin);
          _currentPlugin->addContext(map["name"], _currentContext);
          return;
        }

        if (el == "param" && _currentContext) {
          std::string pname = map["name"];
          std::string ptype = map["type"];

          _currentParam = _currentContext->paramDefine(ptype.c_str(), pname.c_str());
          return;
        }

        if (el == "clip" && _currentContext) {
          std::string cname = map["name"];

          _currentClip = new Attribute::ClipImageDescriptor(cname);
          _currentContext->addClip(cname, _currentClip);
          return;
        }

        if (_currentContext && _currentParam) {
          APICache::propertySetXMLRead(el, map, _currentParam->getEditableProperties(), _currentProp);
          return;
        }

        if (_currentContext && _currentClip) {
          APICache::propertySetXMLRead(el, map, _currentClip->getEditableProperties(), _currentProp);
          return;
        }

        if (!_currentContext && !_currentParam) {
          APICache::propertySetXMLRead(el, map, _currentPlugin->getDescriptor().getEditableProperties(), _currentProp);
          return;
        }

        std::cout << "element " << el << "\n";
        assert(false);
      }

      void PluginCache::xmlCharacterHandler(const std::string &) {
      }

      void PluginCache::xmlElementEnd(const std::string &el) {
        if (el == "param") {
          _currentParam = 0;
        }

        if (el == "context") {
          _currentContext = 0;
        }
      }

      void PluginCache::endXmlParsing() {
        _currentPlugin = 0;
      }

      void PluginCache::saveXML(Plugin *ip, std::ostream &os) const {
        ImageEffectPlugin *p = dynamic_cast<ImageEffectPlugin*>(ip);
        p->saveXML(os);
      }

      void PluginCache::confirmPlugin(Plugin *p) {
        ImageEffectPlugin *plugin = dynamic_cast<ImageEffectPlugin*>(p);
        _plugins.push_back(plugin);

        if (_pluginsByID.find(plugin->getIdentifier()) != _pluginsByID.end()) {
          ImageEffectPlugin *otherPlugin = _pluginsByID[plugin->getIdentifier()];
          if (plugin->trumps(otherPlugin)) {
            _pluginsByID[plugin->getIdentifier()] = plugin;
          }
        } else {
          _pluginsByID[plugin->getIdentifier()] = plugin;
        }

        MajorPlugin maj(plugin);

        if (_pluginsByIDMajor.find(maj) != _pluginsByIDMajor.end()) {
          ImageEffectPlugin *otherPlugin = _pluginsByIDMajor[maj];
          if (plugin->trumps(otherPlugin)) {
            _pluginsByIDMajor[maj] = plugin;
          }
        } else {
          _pluginsByIDMajor[maj] = plugin;
        }
      }

      Plugin *PluginCache::newPlugin(PluginBinary *pb,
        int pi,
        OfxPlugin *pl) {
          ImageEffectPlugin *plugin = new ImageEffectPlugin(*this, pb, pi, pl);
          return plugin;
      }

      Plugin *PluginCache::newPlugin(PluginBinary *pb,
        int pi,
        const std::string &api,
        int apiVersion,
        const std::string &pluginId,
        const std::string &rawId,
        int pluginMajorVersion,
        int pluginMinorVersion) 
      {
        ImageEffectPlugin *plugin = new ImageEffectPlugin(*this, pb, pi, api, apiVersion, pluginId, rawId, pluginMajorVersion, pluginMinorVersion);
        return plugin;
      }

      void PluginCache::dumpToStdOut()
      {
        if (_pluginsByID.empty())
          std::cout << "No Plug-ins Found." << std::endl;

        std::cout << "________________________________________________________________________________" << std::endl;
        for(std::map<std::string, ImageEffectPlugin *>::const_iterator it =  _pluginsByID.begin(); it != _pluginsByID.end(); ++it)
        {
          std::cout << "Plug-in:" << it->first << std::endl;
          std::cout << "\t" << "Filepath: " << it->second->getBinary()->getFilePath();
          std::cout<< "(" << it->second->getIndex() << ")" << std::endl;

          std::cout << "Contexts:" << std::endl;
          const std::set<std::string>& contexts = it->second->getContexts();
          for (std::set<std::string>::const_iterator it2 = contexts.begin(); it2 != contexts.end(); ++it2)
            std::cout << "\t* " << *it2 << std::endl;
          const Descriptor& d =  it->second->getDescriptor();
          std::cout << "Inputs:" << std::endl;
          const std::map<std::string, Attribute::ClipImageDescriptor*>& inputs = d.getClips();
          for (std::map<std::string, Attribute::ClipImageDescriptor*>::const_iterator it2 = inputs.begin(); it2 != inputs.end(); ++it2)
            std::cout << "\t\t* " << it2->first << std::endl;
          std::cout << "________________________________________________________________________________" << std::endl;
        }
      }

    } // ImageEffect
      
  } // Host

} // OFX
