#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/xml/xml.h>

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <stdexcept>
#include <sstream>

#include <boost/assign/list_inserter.hpp>

#include "trim.h"

// this becomes available in standard  C++20
inline bool ends_with(std::string const & value, std::string const & ending)
{
   if (ending.size() > value.size()) return false;
   return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

namespace wxFB
{

struct Property
{
   Property(const wxXmlNode& node);
   std::string name;
   std::string value;
};

struct Object;

typedef std::vector<std::shared_ptr<Object> > Objects;
typedef std::vector<std::shared_ptr<Property> > Properties;

struct Object
{
   Object(const wxXmlNode& node, const wxXmlNode& nodeRoot);
   int depth;
   int lineNumber;
   std::string className;
   bool expanded;
   Objects children;
   Properties properties;

   std::string getProperty(const std::string& name) const;
   int getIntProperty(const std::string& name) const;
   void checkSizerFlags();
   bool isSizerType() const { return ends_with(className, "Sizer"); }
   bool isGridSizerType() const { return ends_with(className, "GridSizer"); }
   bool isBoxSizerType() const { return ends_with(className, "BoxSizer"); }
   int getFlags() const;
   void assertValidSizerFlags() const;
   void showInvalidFlags(const std::string& msg) const;
};


struct Project
{
   Objects objects;
   Project(const wxXmlNode& nodeRoot);
};

}

class CheckSizerFlagsApp : public wxAppConsole
{
   void OnInitCmdLine(wxCmdLineParser& parser) override;
   int OnRun() override;
};

wxIMPLEMENT_APP_CONSOLE(CheckSizerFlagsApp);

void CheckSizerFlagsApp::OnInitCmdLine(wxCmdLineParser& parser)
{
   wxAppConsole::OnInitCmdLine(parser);

   static const wxCmdLineEntryDesc cmdLineDesc[] =
   {
      {
         wxCMD_LINE_PARAM,
         "","","a wxFormBuilder XML project file (.fbp)",
         wxCMD_LINE_VAL_STRING,
         wxCMD_LINE_OPTION_MANDATORY // wxCMD_LINE_PARAM_MULTIPLE
      },

      wxCMD_LINE_DESC_END
   };

   parser.SetDesc(cmdLineDesc);
}

wxFB::Object::Object(const wxXmlNode& nodeObject, const wxXmlNode& nodeRoot)
{
   depth = nodeObject.GetDepth(const_cast<wxXmlNode*>(&nodeRoot));
   lineNumber = nodeObject.GetLineNumber();
   for (const wxXmlAttribute* attr = nodeObject.GetAttributes();
        attr;
        attr = attr->GetNext())
   {
      if ("class" == attr->GetName())
         className = attr->GetValue();
      else if ("expanded" == attr->GetName())
         expanded = "true" == attr->GetValue();
      else
         std::cout << "\nUnrecognized object node attribute " << attr->GetName() << '\n';
   }

   for (const wxXmlNode* nodeChild = nodeObject.GetChildren();
        nodeChild;
        nodeChild = nodeChild->GetNext())
   {
      if ("object" == nodeChild->GetName())
         children.emplace_back(new wxFB::Object(*nodeChild, nodeRoot));
      else if ("property" == nodeChild->GetName())
         properties.emplace_back(new wxFB::Property(*nodeChild));
      else if ("event" == nodeChild->GetName())
         ; // ignored
      else
         std::cout << "\nUnrecognized node name " << nodeChild->GetName() << '\n';
   }
}

wxFB::Property::Property(const wxXmlNode& nodeProperty)
{
   //std::cout << 'p'; // progress indicator
   for (const wxXmlAttribute* attr = nodeProperty.GetAttributes();
        attr;
        attr = attr->GetNext())
   {
      if ("name" == attr->GetName())
         name = attr->GetValue();
      else
         std::cout << "\nUnrecognized property node attribute " << attr->GetName() << '\n';
   }
   value = nodeProperty.GetNodeContent();
}

wxFB::Project::Project(const wxXmlNode& nodeRoot)
{
   for (const wxXmlNode* nodeChild = nodeRoot.GetChildren();
        nodeChild;
        nodeChild = nodeChild->GetNext())
      if ("FileVersion" == nodeChild->GetName())
         ; // ignore
      else if ("object" == nodeChild->GetName())
         objects.emplace_back(new wxFB::Object(*nodeChild, nodeRoot));
      else
         std::cout << "\nUnrecognized node name " << nodeChild->GetName() << '\n';
}

std::ostream& operator <<(std::ostream& os, const wxFB::Object& object)
{
   std::string prefix(object.depth, ' ');
   os << prefix << object.className << '\n';
   prefix += ' ';
   for (auto p : object.properties)
      os << prefix << p->name << " = " << p->value << '\n';
   for (auto p : object.children)
      os << prefix << *p;
   return os;
}

std::ostream& operator <<(std::ostream& os, const wxFB::Project& project)
{
   for (auto p : project.objects)
      os << *p;
   return os;
}

int CheckSizerFlagsApp::OnRun()
{
   const wxString fileName(argv[1]);
   wxXmlDocument doc;
   doc.Load(fileName);
   const wxXmlNode* nodeRoot = doc.GetRoot();
   if ("wxFormBuilder_Project" != nodeRoot->GetName())
      throw std::runtime_error("argument is not a wxFormBuilder project file");
   // build the object tree
   wxFB::Project project(*nodeRoot);
   for (auto p : project.objects)
      p->checkSizerFlags();
   return 0;
}

void wxFB::Object::checkSizerFlags()
{
   // see the asserts in wxWidgets/src/common/sizer.cpp
   if (isSizerType())
      assertValidSizerFlags();
   // see wxGridSizer::DoInsert
   if (isGridSizerType())
   {
      // check for too many children for fixed column+row count
      const int rows = getIntProperty("rows");
      const int cols = getIntProperty("cols");
      if ((0 != rows) && (0 != cols))
      {
         const int capacity = rows * cols;
         if (children.size() > capacity)
            showInvalidFlags("too many children in wxGridSizer");
      }
      // check for children with EXPAND to be able to expand
      for (auto p : children)
      {
         const int flags = p->getFlags();
         if (flags & wxEXPAND)
         {
            const bool ok = !(flags & (wxALIGN_BOTTOM | wxALIGN_CENTRE_VERTICAL)) || 
                            !(flags & (wxALIGN_RIGHT | wxALIGN_CENTRE_HORIZONTAL));
            if (!ok)
               p->showInvalidFlags("wxEXPAND flag in child sizer will be overridden by alignment flags, remove "
                                   "either wxEXPAND or the alignment in at least one direction");
         }
      }
   }
   if (isBoxSizerType())
   {
      // see wxBoxSizer::DoInsert
      const std::string orient = getProperty("orient");
      const bool isVertical = "wxVERTICAL" == orient;
      const bool isHorizontal = "wxHORIZONTAL" == orient;
      for (auto p : children)
      {
         const int flags = p->getFlags();
         if (isVertical)
         {
            static const std::string msg("only horizontal alignment flags can be used in child sizers of vertical box sizers");
            if (wxALIGN_BOTTOM & flags)
               p->showInvalidFlags(msg);
            if (!(flags & wxALIGN_CENTRE_HORIZONTAL))
               if (flags & wxALIGN_CENTRE_VERTICAL)
                  p->showInvalidFlags(msg);
         }
         else if (isHorizontal)
         {
            static const std::string msg("only vertical alignment flags can be used in child sizers of horizontal box sizers");
            if (wxALIGN_RIGHT & flags)
               p->showInvalidFlags(msg);
            if (!(flags & wxALIGN_CENTRE_VERTICAL))
               if (flags & wxALIGN_CENTRE_HORIZONTAL)
                  p->showInvalidFlags(msg);
         }
         else
            p->showInvalidFlags("missing orient property in wxBoxSizer");
         if ( (flags & wxEXPAND) && !(flags & wxSHAPED) )
            if (flags & (wxALIGN_RIGHT | wxALIGN_CENTRE_HORIZONTAL |
                         wxALIGN_BOTTOM | wxALIGN_CENTRE_VERTICAL))
               p->showInvalidFlags("wxEXPAND overrides alignment flags in box sizers");
      }
   }
   for (auto p : children)
      p->checkSizerFlags();
}

static std::string to_hex_string(int v)
{
   std::ostringstream ss;
   ss << "0x" << std::hex << v;
   return ss.str();
}

void wxFB::Object::assertValidSizerFlags() const
{
   static const int SIZER_FLAGS_MASK
         = 0
           | wxCENTRE
           | wxHORIZONTAL
           | wxVERTICAL
           | wxLEFT
           | wxRIGHT
           | wxUP
           | wxDOWN
           | wxALIGN_NOT
           | wxALIGN_CENTER_HORIZONTAL
           | wxALIGN_RIGHT
           | wxALIGN_BOTTOM
           | wxALIGN_CENTER_VERTICAL
           | wxFIXED_MINSIZE
           | wxRESERVE_SPACE_EVEN_IF_HIDDEN
           | wxSTRETCH_NOT
           | wxSHRINK
           | wxGROW
           | wxSHAPED;
   const int flags = getFlags();
   if ((flags & SIZER_FLAGS_MASK) != flags)
      showInvalidFlags("invalid flags not within " + to_hex_string(flags));
}

// this returns a vector of string_views so essentially pointers into
// the original string for efficiency. With an empty input string, the
// result will include a single empty string.

static auto splitString(const std::string& in, char sep)
{
   std::vector<std::string> r;
   r.reserve(std::count(in.begin(), in.end(), sep) + 1); // optional
   for (auto p = in.begin();; ++p) {
      auto q = p;
      p = std::find(p, in.end(), sep);
      r.emplace_back(q, p);
      if (p == in.end())
         return r;
   }
}

static std::map<std::string, int> flagNameMap;

static void initFlagNameMap()
{
   if (0 == flagNameMap.size())
      boost::assign::insert(flagNameMap)
            ("wxCENTRE", wxCENTRE)
            ("wxHORIZONTAL", wxHORIZONTAL)
            ("wxVERTICAL", wxVERTICAL)
            ("wxLEFT", wxLEFT)
            ("wxRIGHT", wxRIGHT)
            ("wxUP", wxUP)
            ("wxDOWN", wxDOWN)
            ("wxALIGN_NOT", wxALIGN_NOT)
            ("wxALIGN_CENTER_HORIZONTAL", wxALIGN_CENTER_HORIZONTAL)
            ("wxALIGN_RIGHT", wxALIGN_RIGHT)
            ("wxALIGN_BOTTOM", wxALIGN_BOTTOM)
            ("wxALIGN_CENTER_VERTICAL", wxALIGN_CENTER_VERTICAL)
            ("wxFIXED_MINSIZE", wxFIXED_MINSIZE)
            ("wxRESERVE_SPACE_EVEN_IF_HIDDEN", wxRESERVE_SPACE_EVEN_IF_HIDDEN)
            ("wxSTRETCH_NOT", wxSTRETCH_NOT)
            ("wxSHRINK", wxSHRINK)
            ("wxGROW", wxGROW)
            ("wxSHAPED", wxSHAPED)
            ("wxEXPAND", wxEXPAND)
            ("wxALL", wxALL)
            ;
}

int wxFB::Object::getFlags() const
{
   initFlagNameMap();
   // can't use a temporary because splitString will return pointers
   // into this
   const std::string flagsProperty = getProperty("flag");
   auto flagNames = splitString(flagsProperty, '|');
   int flags = 0;
   for (auto flagName : flagNames)
   {
      stackoverflow::trim(flagName); // trim whitespace
      if ("" == flagName)
         continue;
      auto p = flagNameMap.find(flagName);
      if (flagNameMap.end() == p)
         showInvalidFlags("unknown flag " + flagName);
      else
         flags |= p->second;
   }
   return flags;
}

void wxFB::Object::showInvalidFlags(const std::string& msg) const
{
   std::cout << "Object " << className << " at  line " << lineNumber << ": " << msg << '\n';
}

std::string wxFB::Object::getProperty(const std::string& name) const
{
   for (auto p : properties)
      if (name == p->name)
         return p->value;
   return "";
}

int wxFB::Object::getIntProperty(const std::string& name) const
{
   const std::string value = getProperty(name);
   return std::stoi(value);
}
