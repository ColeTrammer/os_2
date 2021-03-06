#include <app/box_layout.h>
#include <app/table_view.h>

#include "process_model.h"
#include "process_tab.h"

ProcessTab::ProcessTab(SharedPtr<ProcessModel> model) : m_model(move(model)) {}

void ProcessTab::initialize() {
    auto& layout = set_layout<App::VerticalBoxLayout>();
    layout.set_margins({ 0, 0, 0, 0 });

    auto& tabel = layout.add<App::TableView>();
    tabel.set_model(m_model);
}

ProcessTab::~ProcessTab() {}
