# wxCheckSizerFlags

Check wxFormBuilder project files for potential sizer flag conflicts.

[wxWidgets](https://wxwidgets.org/)' sizers have many control flags. The [wxFormBuilder](https://github.com/wxFormBuilder/wxFormBuilder) GUI designer can potentially create a project with conflicting sizer flags. This utility produces a report of all such conflicts, listing the line number and XML path to the offending object.

Usage: wxCheckSizerFlags example.fbp
