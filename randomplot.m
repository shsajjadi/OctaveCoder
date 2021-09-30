function randomplot(x,z)
  f1=figure(1);
  tic
  for q=1:z
    y=randn(1,x);
    plot(y);
    saveas(f1,'caca.jpg','jpeg')
  end
  toc
end