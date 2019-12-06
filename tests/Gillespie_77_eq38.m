% Source: Matlab user forum discussion
% https://www.mathworks.com/matlabcentral/answers/224604-implementing-gillespie-s-algorithm
% by Harley Day on 11 Feb 2019
opengl('AUTOSELECT');
lineStyles = ['-', '--', ':', '-.'];
markers = ['+', 'o', '*', '.', 'x', 's', 'd', '^', 'v', '>', '<', 'p', 'h'];
colors = ['r', 'g', 'b', 'c', 'm', 'y', 'k'];

%initPop = [3000 3000; 1000 1000; 100 100; 10 10];
initPop = [1000 1000;];
for i=1:size(initPop, 1)
  h1 = figure;
  hold on;
  [t, Y1, Y2] = eqn38 ( initPop(i, :) );
  c1 = colors(mod(i-1, length(colors))+1);
  c2 = colors(mod(i, length(colors))+1);
  h1 = stairs(t, Y1, ...
             'LineWidth', 0.4, ...
             'Color', c1, ...
             'MarkerSize', 0.5, ...
             'Marker', markers(mod(i-1, length(markers))+1), ...
             'MarkerFaceColor', c1, ...
             'MarkerEdgeColor', c1);

  h1 = stairs(t, Y2, ...
             'LineWidth', 0.4, ...
             'Color', c2, ...
             'MarkerSize', 0.5, ...
             'Marker', markers(mod(i, length(markers))+1), ...
             'MarkerFaceColor', c2, ...
             'MarkerEdgeColor', c2);
  grid on;
  hold off;
  xlim([0 30]);
  xlabel('Minutes');
  ylabel('Number of molecules');

  legendText1{2*i-1} = ['$Y1$, $Y1_0$ = [' num2str(initPop(i, 1)) ']'];
  legendText1{2*i} = ['$Y2$, $Y2_0$ = [' num2str(initPop(i, 2)) ']'];
  h1 = legend( legendText1, 'interpreter', 'latex' );

  h2 = figure;
  hold on;
  h2 = plot (Y1, Y2, ...
             'LineWidth', 0.4, ...
             'Color', c1, ...
             'MarkerSize', 0.5, ...
             'Marker', markers(mod(i-1, length(markers))+1), ...
             'MarkerFaceColor', c1, ...
             'MarkerEdgeColor', c1);
  grid on;
  hold off;
  xlabel('Y1');
  ylabel('Y2');

  legendText2{i} = ['$Y_0$ = [' num2str(initPop(i, 1)) ' ' num2str(initPop(i, 2)) ']'];
  h2 = legend( legendText2, 'interpreter', 'latex' );
end


