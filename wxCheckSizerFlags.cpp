#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/xml/xml.h>

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

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
   Object(const wxXmlNode& node, unsigned depth);
   unsigned depth;
   std::string className;
   bool expanded;
   Objects children;
   Properties properties;
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

wxFB::Object::Object(const wxXmlNode& nodeObject, unsigned depth_) :
    depth(depth_)
{
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
         children.emplace_back(new wxFB::Object(*nodeChild, 1 + depth));
      else if ("property" == nodeChild->GetName())
         properties.emplace_back(new wxFB::Property(*nodeChild));
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
         objects.emplace_back(new wxFB::Object(*nodeChild, 0));
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
      std::cout << *p;
   return 0;
}
