#include "FileBrowser.hpp"
#include "../libs/chesto/src/Button.hpp"
#include "../libs/chesto/src/TextElement.hpp"
#include "../libs/chesto/src/Container.hpp"
#include "FileCard.hpp"
#include "MainDisplay.hpp"
#include "TextQueryPopup.hpp"
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <algorithm>

FileBrowser::FileBrowser(const char* pwd)
{
	update_path(pwd);

	cardsPerRow = SCREEN_WIDTH / FILE_CARD_WIDTH;

	x = ((SCREEN_WIDTH - (cardsPerRow * FILE_CARD_WIDTH)) / 4);
	y = 0;

	listfiles();
}

bool FileBrowser::process(InputEvents* events)
{
	bool updateUI = false;
	if (((MainDisplay*)RootDisplay::mainDisplay)->editorView != NULL)
		return false;

	if (events->isTouchDown())
	{
		touchMode = true;
		selected = -1;
	}

	else if (events->pressed(UP_BUTTON) || events->pressed(DOWN_BUTTON) || events->pressed(LEFT_BUTTON) || events->pressed(RIGHT_BUTTON))
	{
		touchMode = false;

		selected -= events->pressed(UP_BUTTON) * 5;
		selected += events->pressed(DOWN_BUTTON) * 5;
		selected += events->pressed(RIGHT_BUTTON);
		selected -= events->pressed(LEFT_BUTTON);

		updateUI |= true;
	}

	if (selected < 0) selected = 0;
	if (selected >= elements.size() - 2) selected = elements.size() - 3;

	updateUI |= ListElement::process(events);

	// excluding path and buttons at the end, this should be a FileCard
	auto currentCard = (FileCard*)elements[selected];

	int cardY = currentCard->yAbs;

	if (!touchMode) {
		// keep the cursor on screen if using buttons
		if (selected <= 3) {
			this->y = 0;
			updateUI |= true;
		}
		else
		{
			if (cardY < 10) {
				this->y += currentCard->height;
				updateUI |= true;
			}
			if (cardY > SCREEN_HEIGHT - (currentCard->height + 10)) {
				this->y -= currentCard->height;
				updateUI |= true;
			}
		}
	}

	if (events->released(A_BUTTON))
	{
		// activate this file's editor
		currentCard->openMyFile();
		updateUI = true;
	}

	if (events->released(B_BUTTON))
	{
		// activate an upCard if it exists, to go back
		if (upCard != NULL)
			upCard->openMyFile();
		updateUI = true;
	}

	return updateUI;
}

void FileBrowser::render(Element* parent)
{
	if (((MainDisplay*)RootDisplay::mainDisplay)->editorView != NULL)
		return;

	ListElement::render(parent);

	if (selected >= 0 && !touchMode) 
	{
		// draw the cursor for this file
		auto selectedElement = elements[selected];
		CST_Rect dimens4 = selectedElement->getBounds();
		for (int z = 4; z >= 0; z--)
		{
			CST_SetDrawColor(parent->renderer, { 0x66 - z * 10, 0x7c + z * 20, 0x89 + z * 10, 0xFF });
			dimens4.x--;
			dimens4.y--;
			dimens4.w += 2;
			dimens4.h += 2;
			CST_DrawRect(parent->renderer, &dimens4);
		}
	}
}

// copy-pasta'd from Utils.cpp in hb-appstore
// TODO: consolidate
const std::string dir_name(std::string file_path)
{
	// turns "/hi/man/thing.txt to /hi/man"
	int pos = file_path.find_last_of("/");

	// no "/" in string, return empty string
	if (pos == std::string::npos)
		return "";

	return file_path.substr(0, pos);
}

void FileBrowser::update_path(const char* pwd)
{
	if (this->pwd)
		delete this->pwd;

	char path[2048];
#if defined(PC)
	// PC can use realpath, other platforms might not have it
	realpath(pwd, path);
	this->pwd = new std::string(path);
#else
	this->pwd = new std::string(pwd);
#endif
}

void FileBrowser::listfiles()
{
	// go through all files in current directory and create FileCard children out of them

	upCard = NULL;
	this->elements.clear();

	DIR* dirp;
	struct dirent* entry;

	dirp = opendir(pwd->c_str());

	int count = 0;

	// create a hardcoded "up" link to the parent directory
	if (*pwd != std::string("/"))
	{
		FileCard* card = new FileCard(true, ".. (parent)");
		card->position(this->x + (count % cardsPerRow) * card->width, this->y + 115 + (count / cardsPerRow) * card->height);
		std::string cwd = dir_name(*pwd);
		card->path = new std::string(cwd == "" ? "/" : cwd);
		card->action = std::bind(&FileCard::openMyFile, card);
		this->elements.push_back(card);
		upCard = card;
		count++;
	}

	if (dirp)
	{
		std::vector<FileCard*> fileCards;
		while ((entry = readdir(dirp)) != NULL)
		{
			// skip the '.' and '..' entries if they exist in the filesystem
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
				continue;

			FileCard* card = new FileCard(entry->d_type == DT_DIR, entry->d_name);
			card->path = new std::string(*pwd + (*pwd != "/" ? std::string("/") : "") + entry->d_name);
			card->action = std::bind(&FileCard::openMyFile, card);
			fileCards.push_back(card);
		}

		// sort alphabetically
		std::sort(fileCards.begin(), fileCards.end(), [](FileCard* a, FileCard* b){
			return strncasecmp(a->path->c_str(), b->path->c_str(), std::min(a->path->length(), b->path->length())) < 0;
		});

		// make children of the browser
		for (auto card : fileCards) {
			child(card);
			card->position(this->x + (count % cardsPerRow) * card->width, this->y + 115 + (count / cardsPerRow) * card->height);
			count++;
		}

		closedir(dirp);
	}

	// current path at the top
	CST_Color white = { 0xFF, 0xFF, 0xFF, 0xFF };

	// limit it to 45 chars
	std::string clippedPath(*pwd);
	if (clippedPath.length() > 45) {
		clippedPath.erase(clippedPath.begin(), clippedPath.end() - 45);
		clippedPath.insert(0, "…");
	}

	TextElement* path = new TextElement(clippedPath.c_str(), 25, &white, MONOSPACED);
	path->position(10, 30);
	this->elements.push_back(path);

	auto mainDisplay = RootDisplay::mainDisplay;

	// new folder, file, and exit buttons
	Container* con = new Container(ROW_LAYOUT, 10);
	con->add((new Button("Exit", SELECT_BUTTON, true))->setAction([mainDisplay](){
		mainDisplay->exitRequested = true;
		mainDisplay->isRunning = false;
	}));

	con->add((new Button("New Folder", X_BUTTON, true))->setAction([this, mainDisplay](){
		std::function<void(const char*)> createFunc = [this](const char* name){
			printf("Creating folder [%s]\n", name);
			::mkdir((*this->pwd + "/" + name).c_str(), 0775);
			this->listfiles();
		};
		mainDisplay->switchSubscreen(new TextQueryPopup("Enter new folder name", "Create", createFunc));
	}));

	con->add((new Button("New File", Y_BUTTON, true))->setAction([this, mainDisplay](){
		std::function<void(const char*)> createFunc = [this](const char* name){
			printf("Creating file [%s]\n", name);
			std::ofstream output((*this->pwd + "/" + name).c_str());
			this->listfiles();
		};
		mainDisplay->switchSubscreen(new TextQueryPopup("Enter new file name", "Create", createFunc));
	}));

	con->position(SCREEN_WIDTH - con->width - this->x * 2, 30);
	this->elements.push_back(con);
}