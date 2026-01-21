#include <QDialog>

namespace elgatocloud {

class ElgatoUpdateModal : public QDialog {
	Q_OBJECT

public:
	explicit ElgatoUpdateModal(QWidget *parent, std::string version, std::string downloadUrl);
	~ElgatoUpdateModal();

private:

};

void openUpdateModal(std::string version, std::string downloadUrl);
}
