package com.smartev.dashboard.ui

import android.graphics.Color
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ProgressBar
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import com.smartev.dashboard.R

class BmsCellAdapter : RecyclerView.Adapter<BmsCellAdapter.CellViewHolder>() {

    private var cells: List<Int> = emptyList()
    private var minIndex: Int = -1
    private var maxIndex: Int = -1

    fun updateCells(newCells: List<Int>, minIdx: Int, maxIdx: Int) {
        this.cells = newCells
        this.minIndex = minIdx
        this.maxIndex = maxIdx
        notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CellViewHolder {
        val view = LayoutInflater.from(parent.context).inflate(R.layout.item_bms_cell, parent, false)
        return CellViewHolder(view)
    }

    override fun onBindViewHolder(holder: CellViewHolder, position: Int) {
        val cellIndex = position + 1
        val voltageMv = if (position < cells.size) cells[position] else 0

        holder.tvCellIndex.text = String.format("#%02d", cellIndex)
        holder.tvCellVoltage.text = String.format("%.3f V", voltageMv / 1000f)
        holder.pbCellVoltage.progress = voltageMv

        // Đổi màu làm nổi bật cell min/max
        when (cellIndex) {
            minIndex -> {
                holder.tvCellVoltage.setTextColor(Color.parseColor("#FF5252")) // Đỏ cảnh báo cell yếu nhất
                holder.tvCellIndex.text = String.format("#%02d (MIN)", cellIndex)
            }
            maxIndex -> {
                holder.tvCellVoltage.setTextColor(Color.parseColor("#00E5FF")) // Cyan cell cao nhất
                holder.tvCellIndex.text = String.format("#%02d (MAX)", cellIndex)
            }
            else -> {
                holder.tvCellVoltage.setTextColor(Color.parseColor("#F0F6FC")) // Trắng bình thường
            }
        }
    }

    override fun getItemCount(): Int = cells.size

    class CellViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        val tvCellIndex: TextView = itemView.findViewById(R.id.tvCellIndex)
        val tvCellVoltage: TextView = itemView.findViewById(R.id.tvCellVoltage)
        val pbCellVoltage: ProgressBar = itemView.findViewById(R.id.pbCellVoltage)
    }
}
