#include "../libs/chesto/src/ListElement.hpp"
#include <string>

class FileCard;

char* my_realpath(const char* path, char resolved_path[]);

class FileBrowser : public ListElement
{
public:
	FileBrowser(const char* pwd);
	bool process(InputEvents* event);
	void render(Element* parent);
	void listfiles();
	void update_path(const char* path);

	FileCard* upCard = NULL;

	std::string* pwd = NULL;
	bool touchMode = true;
	int cardsPerRow = 5;

	int selected = -1;
};